## 1. 安装信号handler



# 第一次启动

## 1. 创建系统需要的文件系统

init会创建并挂载系统启动所需的文件目录，编译Android系统源码时，并不存在/dev、/proc、/sys等目录，它们是系统运行时由init进行生成的。在Android上，init扮演着Linux系统上udev守护进程的角色。

```c++
mount("tmpfs", "/dev", "tmpfs", MS_NOSUID, "mode=0755");
mkdir("/dev/pts", 0755);
mkdir("/dev/socket", 0755);
mount("devpts", "/dev/pts", "devpts", 0, NULL);
#define MAKE_STR(x) __STRING(x)
mount("proc", "/proc", "proc", 0, "hidepid=2,gid=" MAKE_STR(AID_READPROC));
// Don't expose the raw commandline to unprivileged processes.
chmod("/proc/cmdline", 0440);
gid_t groups[] = { AID_READPROC };
setgroups(arraysize(groups), groups);
mount("sysfs", "/sys", "sysfs", 0, NULL);
mount("selinuxfs", "/sys/fs/selinux", "selinuxfs", 0, NULL);
mknod("/dev/kmsg", S_IFCHR | 0600, makedev(1, 11));
mknod("/dev/random", S_IFCHR | 0666, makedev(1, 8));
mknod("/dev/urandom", S_IFCHR | 0666, makedev(1, 9));
```



## 2. 初始化kernel log

```C++
void InitKernelLogging(char* argv[]) {
    // Make stdin/stdout/stderr all point to /dev/null.
    int fd = open("/sys/fs/selinux/null", O_RDWR);
    if (fd == -1) {
        int saved_errno = errno;
        android::base::InitLogging(argv, &android::base::KernelLogger);
        errno = saved_errno;
        PLOG(FATAL) << "Couldn't open /sys/fs/selinux/null";
    }
    dup2(fd, 0);
    dup2(fd, 1);
    dup2(fd, 2);
    if (fd > 2) close(fd);

    android::base::InitLogging(argv, &android::base::KernelLogger);
}
```

`InitLogging`的核心工作就是 打卡`/dev/kmsg`节点，此后，往该节点写入数据就可以由内核打印出来。类似于`printk`。其log长度限制为1024字节。



## 3.完成第一阶段的剩余操作
不知道啥操作，哈哈哈。
```c++
if (!DoFirstStageMount()) {
	LOG(ERROR) << "Failed to mount required partitions early ...";
	panic();
}

SetInitAvbVersionInRecovery();

// Set up SELinux, loading the SELinux policy.
selinux_initialize(true);

// We're in the kernel domain, so re-exec init to transition to the init domain now
// that the SELinux policy has been loaded.
if (selinux_android_restorecon("/init", 0) == -1) {
	PLOG(ERROR) << "restorecon failed";
	security_failure();
}
```

## 4. 准备第二次启动。
```c++
setenv("INIT_SECOND_STAGE", "true", 1);
static constexpr uint32_t kNanosecondsPerMillisecond = 1e6;
uint64_t start_ms = start_time.time_since_epoch().count() / kNanosecondsPerMillisecond;
setenv("INIT_STARTED_AT", std::to_string(start_ms).c_str(), 1);

char* path = argv[0];
char* args[] = { path, nullptr };
execv(path, args);
```



# 第二次启动

首先重新初始化kernel log。

1. [Linux 密钥保留服务入门](https://www.ibm.com/developerworks/cn/linux/l-key-retention.html?S_TACT=105AGX52&S_CMP=techcsdn)

    ```c+=
    // Set up a session keyring that all processes will have access to. It
    // will hold things like FBE encryption keys. No process should override
    // its session keyring.
    keyctl_get_keyring_ID(KEY_SPEC_SESSION_KEYRING, 1);
    ```

2. 初始化属性服务

    ```c++
    property_init();
    ```

3. 解析内核启动时传入的arguments。

    ```c++
    // If arguments are passed both on the command line and in DT,
    // properties set in DT always have priority over the command-line ones.
    process_kernel_dt();
    process_kernel_cmdline();
    // Propagate the kernel variables to internal variables
    // used by init as well as the current required properties.
    export_kernel_boot_props();
    ```

4. 初始化selinux

    ```c++
    // Now set up SELinux for second stage.
    selinux_initialize(false);
    selinux_restore_context();
    ```

5. 初始化信号handler

    ```c++
    signal_handler_init();
    ```

6. 不知道干啥

    ```c++
     property_load_boot_defaults();
        export_oem_lock_status();
        start_property_service();
        set_usb_controller();
    ```

7. 解析init.rc

    ```c++
        const BuiltinFunctionMap function_map;
        Action::set_function_map(&function_map);
    
        ActionManager& am = ActionManager::GetInstance();
        ServiceManager& sm = ServiceManager::GetInstance();
        Parser& parser = Parser::GetInstance();
    
        parser.AddSectionParser("service", std::make_unique<ServiceParser>(&sm));
        parser.AddSectionParser("on", std::make_unique<ActionParser>(&am));
        parser.AddSectionParser("import", std::make_unique<ImportParser>(&parser));
        std::string bootscript = GetProperty("ro.boot.init_rc", "");
        if (bootscript.empty()) {
            parser.ParseConfig("/init.rc");
            parser.set_is_system_etc_init_loaded(
                    parser.ParseConfig("/system/etc/init"));
            parser.set_is_vendor_etc_init_loaded(
                    parser.ParseConfig("/vendor/etc/init"));
            parser.set_is_odm_etc_init_loaded(parser.ParseConfig("/odm/etc/init"));
        } else {
            parser.ParseConfig(bootscript);
            parser.set_is_system_etc_init_loaded(true);
            parser.set_is_vendor_etc_init_loaded(true);
            parser.set_is_odm_etc_init_loaded(true);
        }
    
        // Turning this on and letting the INFO logging be discarded adds 0.2s to
        // Nexus 9 boot time, so it's disabled by default.
        if (false) DumpState();
    
        am.QueueEventTrigger("early-init");
    
        // Queue an action that waits for coldboot done so we know ueventd has set up all of /dev...
        am.QueueBuiltinAction(wait_for_coldboot_done_action, "wait_for_coldboot_done");
        // ... so that we can start queuing up actions that require stuff from /dev.
        am.QueueBuiltinAction(mix_hwrng_into_linux_rng_action, "mix_hwrng_into_linux_rng");
        am.QueueBuiltinAction(set_mmap_rnd_bits_action, "set_mmap_rnd_bits");
        am.QueueBuiltinAction(set_kptr_restrict_action, "set_kptr_restrict");
        am.QueueBuiltinAction(keychord_init_action, "keychord_init");
        am.QueueBuiltinAction(console_init_action, "console_init");
    
        // Trigger all the boot actions to get us started.
        am.QueueEventTrigger("init");
    
        // Repeat mix_hwrng_into_linux_rng in case /dev/hw_random or /dev/random
        // wasn't ready immediately after wait_for_coldboot_done
        am.QueueBuiltinAction(mix_hwrng_into_linux_rng_action, "mix_hwrng_into_linux_rng");
    
        // Don't mount filesystems or start core system services in charger mode.
        std::string bootmode = GetProperty("ro.boot.mode", "");
        if (bootmode == "charger") {
            am.QueueEventTrigger("charger");
        } else {
            am.QueueEventTrigger("late-init");
        }
    
        // Run all property triggers based on current state of the properties.
        am.QueueBuiltinAction(queue_property_triggers_action, "queue_property_triggers");
    ```

8. 进入循环。

    