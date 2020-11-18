# Vold 解析fstab文件

## fstab 文件

```ini
# Android fstab file.
#<src>                  <mnt_point> <type>  <mnt_flags and options>       				<fs_mgr_flags>
# The filesystem that contains the filesystem checker binary (typically /system) cannot
# specify MF_CHECK, and must come before any filesystems that do specify MF_CHECK

/dev/block/rknand_system  /system    ext4   ro,noatime,nodiratime,noauto_da_alloc,discard   wait,resize
/devices/10180000.usb*                      auto     vfat      defaults        voldmanaged=usb:auto
```

注意上面的例子，只有 `fs_mgr_flags`设置了 `voldmanaged=`的才会有Vold 管理.

The voldmanaged flag is followed by an = and the label, a colon and the partition number or the word "auto", e.g.  voldmanaged=sdcard:3

## 数据结构

`struct fstab`管理整个fstab，每个有效行用`struct fstab_rec`表示。

```c
struct fstab {
    int num_entries;			// 有效行数目。
    struct fstab_rec *recs;		
    char *fstab_filename;	// fstab配置文件路径
};

struct fstab_rec {
    char *blk_device;
    char *mount_point;
    char *fs_type;
    unsigned long flags;
    char *fs_options;
    int fs_mgr_flags;
    char *key_loc;
    char *verity_loc;
    long long length;
    char *label;
    int partnum;
    int swap_prio;
    unsigned int zram_size;
    unsigned int file_encryption_mode;
};
```

#### fs_mgr_flags



```c++

static struct flag_list fs_mgr_flags[] = {
    { "wait",        MF_WAIT },
    { "check",       MF_CHECK },
    { "encryptable=",MF_CRYPT },
    { "forceencrypt=",MF_FORCECRYPT },
    { "fileencryption=",MF_FILEENCRYPTION },
    { "forcefdeorfbe=",MF_FORCEFDEORFBE },
    { "nonremovable",MF_NONREMOVABLE },
    { "voldmanaged=",MF_VOLDMANAGED},
    { "length=",     MF_LENGTH },
    { "recoveryonly",MF_RECOVERYONLY },
    { "swapprio=",   MF_SWAPPRIO },
    { "zramsize=",   MF_ZRAMSIZE },
    { "verifyatboot", MF_VERIFYATBOOT },
    { "verify",      MF_VERIFY },
    { "noemulatedsd", MF_NOEMULATEDSD },
    { "notrim",       MF_NOTRIM },
    { "formattable", MF_FORMATTABLE },
    { "slotselect",  MF_SLOTSELECT },
    { "nofail",      MF_NOFAIL },
    { "latemount",   MF_LATEMOUNT },
    { "defaults",    0 },
    { 0,             0 },
};
```



#### mount_flags

```c++
static struct flag_list mount_flags[] = {
    { "noatime",    MS_NOATIME },
    { "noexec",     MS_NOEXEC },
    { "nosuid",     MS_NOSUID },
    { "nodev",      MS_NODEV },
    { "nodiratime", MS_NODIRATIME },
    { "ro",         MS_RDONLY },
    { "rw",         0 },
    { "remount",    MS_REMOUNT },
    { "bind",       MS_BIND },
    { "rec",        MS_REC },
    { "unbindable", MS_UNBINDABLE },
    { "private",    MS_PRIVATE },
    { "slave",      MS_SLAVE },
    { "shared",     MS_SHARED },
    { "defaults",   0 },
    { 0,            0 },
};
```



### 解析过程 

> system/core/fs_mgr/fs_mgr_fstab.c @ `fs_mgr_read_fstab`  

1. 获取一行数据

2. 通过`strtok_r`函数分割数据。分隔符为空格或者制表符

3. 将数据赋值到 `fstab_rec`

   这里还包括解析 `mnt_flags` 和 `fs_mgr_flags`

数据解析完后，有这么一段代码：

```c++
/* If an A/B partition, modify block device to be the real block device */
if (fs_mgr_update_for_slotselect(fstab) != 0) {
    ERROR("Error updating for slotselect\n");
    goto err;
}
```

A/B分区？？？现在还没接触到，接触到了再回来看看这段代码干嘛用的，



## Vold 解析 fstab

Vold只关系 `fs_mgr_flags`设置了 `voldmanaged=`的项。

```c++
static int process_config(VolumeManager *vm, bool* has_adoptable) {
	//获取fstab配置文件路径
    std::string path(android::vold::DefaultFstabPath());
    fstab = fs_mgr_read_fstab(path.c_str());
    *has_adoptable = false;
    for (int i = 0; i < fstab->num_entries; i++) {
		// 判断fstab中的路径是否由 Vold 管理。。。
        if (fs_mgr_is_voldmanaged(&fstab->recs[i])) {
            if (fs_mgr_is_nonremovable(&fstab->recs[i])) {
                LOG(WARNING) << "nonremovable no longer supported; ignoring volume";
                continue;
            }

            std::string sysPattern(fstab->recs[i].blk_device);
			//对应 voldmanaged=usb:auto 中的 usb
            std::string nickname(fstab->recs[i].label);
            int flags = 0;

			// fs_msg_flag 中是否设置了 encryptable、forceencrypt、forcefdeorfbe
            if (fs_mgr_is_encryptable(&fstab->recs[i])) {
				// Adoptable 表示 该磁盘可作为内部存储
                flags |= android::vold::Disk::Flags::kAdoptable;
                *has_adoptable = true;
            }
			
			// fs_msg_flag 中设置了 noemulatedsd
            if (fs_mgr_is_noemulatedsd(&fstab->recs[i])
                    || property_get_bool("vold.debug.default_primary", false)) {
                // 默认主存储分区
                flags |= android::vold::Disk::Flags::kDefaultPrimary;
            }
		
            //阅读了我手上的设备的 fstab文件后，该处并未执行
            vm->addDiskSource(std::shared_ptr<VolumeManager::DiskSource>(
                    new VolumeManager::DiskSource(sysPattern, nickname, flags)));
        }
    }
    return 0;
}
```

这里会为设置了`voldmanaged`的项创建一个`DiskSource`对象，并添加到`VolumeManager`中。





# U盘和TF卡是如何被vold管理

这里就涉及到了 Netlink机制。 [参考链接](https://blog.csdn.net/zhao_h/article/details/80943226)

通过Netlink,Vold就能从内核获取到 存储设备的add、remove等消息，并作出相应的处理。

## Netlink处理流程

通过`NetlinkManager`和`NetlinkHandler`。

Vold main函数中`nm->start()`会启动一个线程来接受内核的netlink消息。当数据到来时，就会回到`NetlinkHandler::onEvent`处理。

具体的实现细节可以阅读以下文件源码：

```C++
// NetlinkHandler.cpp  (vold)
// NetlinkMananger.cpp (vold)
// libsysutil (对Unix domain Socket 和 netlink进行了一些封装)
```







