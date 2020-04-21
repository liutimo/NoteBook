---
title: Android-init-Service
date: 2020-04-14 13:53:00
categories: Android Framework
tags: 
    - Android Framework
    - Android Init进程
---

## service 解析

init.rc中，service语法如下所示:

```shell
service ril-daemon /vendor/bin/hw/rild -l /system/lib64/libreference-ril.so
    class main
    socket rild stream 660 root radio 
    socket rild-debug stream 666 radio system 
    user root 
    group radio cache inet misc audio sdcard_rw log
```



不知道怎么清晰的阅读源码，就从init.rc开始吧。首先第一行会被传入到`ServiceParser`中，创建一个`Service`对象。随后第2行`class`。`Service`类中有一个`ParserClass`方法来解析。

```c++
bool Service::ParseClass(const std::vector<std::string>& args, std::string* err) {
    classnames_ = std::set<std::string>(args.begin() + 1, args.end());
    return true;
}
```

将`class`后的所有东西都放到成员`classnames_`这个集合中。

相似的，还有很多`Parser****`一样的函数。

```c++
bool ParseCapabilities(const std::vector<std::string>& args, std::string *err);
bool ParseClass(const std::vector<std::string>& args, std::string* err);
bool ParseConsole(const std::vector<std::string>& args, std::string* err);
bool ParseCritical(const std::vector<std::string>& args, std::string* err);
bool ParseDisabled(const std::vector<std::string>& args, std::string* err);
bool ParseGroup(const std::vector<std::string>& args, std::string* err);
bool ParsePriority(const std::vector<std::string>& args, std::string* err);
bool ParseIoprio(const std::vector<std::string>& args, std::string* err);
bool ParseKeycodes(const std::vector<std::string>& args, std::string* err);
bool ParseOneshot(const std::vector<std::string>& args, std::string* err);
bool ParseOnrestart(const std::vector<std::string>& args, std::string* err);
bool ParseOomScoreAdjust(const std::vector<std::string>& args, std::string* err);
bool ParseMemcgLimitInBytes(const std::vector<std::string>& args, std::string* err);
bool ParseMemcgSoftLimitInBytes(const std::vector<std::string>& args, std::string* err);
bool ParseMemcgSwappiness(const std::vector<std::string>& args, std::string* err);
bool ParseNamespace(const std::vector<std::string>& args, std::string* err);
bool ParseSeclabel(const std::vector<std::string>& args, std::string* err);
bool ParseSetenv(const std::vector<std::string>& args, std::string* err);
bool ParseShutdown(const std::vector<std::string>& args, std::string* err);
bool ParseSocket(const std::vector<std::string>& args, std::string* err);
bool ParseFile(const std::vector<std::string>& args, std::string* err);
bool ParseUser(const std::vector<std::string>& args, std::string* err);
bool ParseWritepid(const std::vector<std::string>& args, std::string* err);
```

选几个再看一哈。



### 解析socket

```c++
// name type perm [ uid gid context ]
bool Service::ParseSocket(const std::vector<std::string>& args, std::string* err) {
    //校验type是否正确
    if (!StartsWith(args[2], "dgram") && !StartsWith(args[2], "stream") &&
        !StartsWith(args[2], "seqpacket")) {
        *err = "socket type must be 'dgram', 'stream' or 'seqpacket'";
        return false;
    }
    return AddDescriptor<SocketInfo>(args, err);
}
```

其实，`File`和`Socket`类似，他们都被抽象成`DescriptorInfo`。

```c++
class DescriptorInfo {
 public:
  DescriptorInfo(const std::string& name, const std::string& type, uid_t uid,
                 gid_t gid, int perm, const std::string& context);
  virtual ~DescriptorInfo();
 private:
  std::string name_;
  std::string type_;
  uid_t uid_;
  gid_t gid_;
  int perm_;
  std::string context_;
    
  virtual int Create(const std::string& globalContext) const = 0;
  virtual const std::string key() const = 0;
};

class SocketInfo : public DescriptorInfo {
 public:
  SocketInfo(const std::string& name, const std::string& type, uid_t uid,
             gid_t gid, int perm, const std::string& context);
  void Clean() const override;
 private:
  virtual int Create(const std::string& context) const override;
  virtual const std::string key() const override;
};

class FileInfo : public DescriptorInfo {
 public:
  FileInfo(const std::string& name, const std::string& type, uid_t uid,
           gid_t gid, int perm, const std::string& context);
 private:
  virtual int Create(const std::string& context) const override;
  virtual const std::string key() const override;
};

```

其中，比较重要的方法就是`DescriptorInfo::Create`。以`SocketInfo`为例，在init.rc中的socket的作用是用于IPC，其实现原理就是UDS(unix domain socket)。关于其具体使用可以参考APUE。

先来看看service是怎么创建socket的：

```c++
int CreateSocket(const char* name, int type, bool passcred, mode_t perm, uid_t uid, gid_t gid,
                 const char* socketcon, selabel_handle* sehandle) {
	//1. 创建socket
    android::base::unique_fd fd(socket(PF_UNIX, type, 0));
    if (fd < 0) {
        PLOG(ERROR) << "Failed to open socket '" << name << "'";
        return -1;
    }

    //2. init sockaddr_un
    struct sockaddr_un addr;
    memset(&addr, 0 , sizeof(addr));
    addr.sun_family = AF_UNIX;
    //此处会在/dev/socket/下生成一个名为 name  的 socket文件， client打开这个文件就可以同service通信了
    snprintf(addr.sun_path, sizeof(addr.sun_path), ANDROID_SOCKET_DIR"/%s", name);
    //3. 判断文件逻辑存不存在
    if ((unlink(addr.sun_path) != 0) && (errno != ENOENT)) {
        PLOG(ERROR) << "Failed to unlink old socket '" << name << "'";
        return -1;
    }

    //4. bind
    int ret = bind(fd, (struct sockaddr *) &addr, sizeof (addr));
}
```

到这里，创建socket已经结束后，后面的流程再接着看：

```c++
void DescriptorInfo::CreateAndPublish(const std::string& globalContext) const {
  // Create
  const std::string& contextStr = context_.empty() ? globalContext : context_;
  int fd = Create(contextStr);
  if (fd < 0) return;

  // #libcutils/include_vndk/cutils/sockets.h:45
  // #define ANDROID_SOCKET_ENV_PREFIX	"ANDROID_SOCKET_"
  // Publish   key() == ANDROID_SOCKET_   
  std::string publishedName = key() + name_;
  std::for_each(publishedName.begin(), publishedName.end(),
                [] (char& c) { c = isalnum(c) ? c : '_'; });

  std::string val = std::to_string(fd);
  //[1]
  add_environment(publishedName.c_str(), val.c_str());

  //[2] make sure we don't close on exec
  fcntl(fd, F_SETFD, 0);
}
```

[1] 处的代码将fd保存到环境变量里去了。[2]处的代码使得fd在exec执行时不会被`close`，这样在执行`exec`后的子进程，通过环境变量(`ANDROID_SOCKET_ + SOCKETNAME`)仍然能拿到这个fd并操作他。

以adbd为例：

```shell
service adbd /system/bin/adbd --root_seclabel=u:r:su:s0
    socket adbd stream 660 system system
    seclabel u:r:adbd:s0		
```

按照init进程，该段rc文件，会生成一个名为`/dev/socket/adbd`的socket文件。并且在init进程中创建一个

`ANDROID_SOCKET_adbd=<fd>`的环境变量。

接下来看看adbd中怎么操作。

```c++
//adbd_auth.cpp
void adbd_auth_init(void) {
    int fd = android_get_control_socket("adbd");
    //监听咯
    if (listen(fd, 4) == -1) {
        return;
    }
	
    fdevent_install(&listener_fde, fd, adbd_auth_listener, NULL);
    fdevent_add(&listener_fde, FDE_READ);
}
```

通过`android_get_control_socket`来获取`exec`前的fd。

```c++
int android_get_control_socket(const char* name) {
    //[1]
    int fd = __android_get_control_from_env(ANDROID_SOCKET_ENV_PREFIX, name);

    if (fd < 0) return fd;

    //[2] Compare to UNIX domain socket name, must match!
    struct sockaddr_un addr;
    socklen_t addrlen = sizeof(addr);
    int ret = TEMP_FAILURE_RETRY(getsockname(fd, (struct sockaddr *)&addr, &addrlen));
    if (ret < 0) return -1;
    char *path = NULL;
    if (asprintf(&path, ANDROID_SOCKET_DIR "/%s", name) < 0) return -1;
    if (!path) return -1;
    int cmp = strcmp(addr.sun_path, path);
    free(path);
    if (cmp != 0) return -1;

    // It is what we think it is
    return fd;
}
```

[1] 处代码就是从环境变量中获取fd。

[2]处代码就是获取fd的socket文件路径，并同根据`name`生成的socket文件路径进行比对，判断是否合法。

如果我们要同adbd进行通信，就打开`/dev/socket/adbd`这个socket文件与之通信即可。



### 解析group



### 如何找到相应的Parser

`Service`在解析`Section`时，和`Action`的方式类似。都是创建一个`map`，然后根据`Section`中的keyword区找到适合的解析函数。

```c++
const Service::OptionParserMap::Map& Service::OptionParserMap::map() const {
    constexpr std::size_t kMax = std::numeric_limits<std::size_t>::max();
    // clang-format off
    static const Map option_parsers = {
        {"capabilities",
                        {1,     kMax, &Service::ParseCapabilities}},
        {"class",       {1,     kMax, &Service::ParseClass}},
        {"console",     {0,     1,    &Service::ParseConsole}},
        {"critical",    {0,     0,    &Service::ParseCritical}},
        {"disabled",    {0,     0,    &Service::ParseDisabled}},
        {"group",       {1,     NR_SVC_SUPP_GIDS + 1, &Service::ParseGroup}},
        {"ioprio",      {2,     2,    &Service::ParseIoprio}},
        {"priority",    {1,     1,    &Service::ParsePriority}},
        {"keycodes",    {1,     kMax, &Service::ParseKeycodes}},
        {"oneshot",     {0,     0,    &Service::ParseOneshot}},
        {"onrestart",   {1,     kMax, &Service::ParseOnrestart}},
        {"oom_score_adjust",
                        {1,     1,    &Service::ParseOomScoreAdjust}},
        {"memcg.swappiness",
                        {1,     1,    &Service::ParseMemcgSwappiness}},
        {"memcg.soft_limit_in_bytes",
                        {1,     1,    &Service::ParseMemcgSoftLimitInBytes}},
        {"memcg.limit_in_bytes",
                        {1,     1,    &Service::ParseMemcgLimitInBytes}},
        {"namespace",   {1,     2,    &Service::ParseNamespace}},
        {"seclabel",    {1,     1,    &Service::ParseSeclabel}},
        {"setenv",      {2,     2,    &Service::ParseSetenv}},
        {"shutdown",    {1,     1,    &Service::ParseShutdown}},
        {"socket",      {3,     6,    &Service::ParseSocket}},
        {"file",        {2,     2,    &Service::ParseFile}},
        {"user",        {1,     1,    &Service::ParseUser}},
        {"writepid",    {1,     kMax, &Service::ParseWritepid}},
    };
    // clang-format on
    return option_parsers;
}

bool Service::ParseLine(const std::vector<std::string>& args, std::string* err) {
    static const OptionParserMap parser_map;
    auto parser = parser_map.FindFunction(args, err);

    if (!parser) {
        return false;
    }
    return (this->*parser)(args, err);
}
```

除去这些解析类的函数外，`Service`中的重头戏应该就是`fork`子进程吧。



## Service是如何被创建的

`Service`类中，比较重要的函数就是关于进程创建那几个吧。其余的基本都是解析init.rc的，就不那么深入了解解析的细节了。

```c++

bool Service::Start() {
    //取消这些标志位
    flags_ &= (~(SVC_DISABLED|SVC_RESTARTING|SVC_RESET|SVC_RESTART|SVC_DISABLED_START));
    //正在启动，就不要启动第二次了
    if (flags_ & SVC_RUNNING) {
        return false;
    }
    //需要操作控制台
    bool needs_console = (flags_ & SVC_CONSOLE);
    if (needs_console) {
        //验证console是否能正常打开
        int console_fd = open(console_.c_str(), O_RDWR | O_CLOEXEC);
        close(console_fd);
    }
    //验证Service对应的执行程序是否存在
    struct stat sb;
    stat(args_[0].c_str(), &sb);
    
    std::string scon;
    if (!seclabel_.empty()) {
        scon = seclabel_;
    } else {
        scon = ComputeContextFromExecutable(name_, args_[0]);
        if (scon == "") {
            return false;
        }
    }

    LOG(INFO) << "starting service '" << name_ << "'...";

    pid_t pid = -1;
    if (namespace_flags_) {
        pid = clone(nullptr, nullptr, namespace_flags_ | SIGCHLD, nullptr);
    } else {
        //走fork吧
        pid = fork();
    }

    if (pid == 0) {
        umask(077);
        if (namespace_flags_ & CLONE_NEWPID) {
            SetUpPidNamespace(name_);
        }
	
        for (const auto& ei : envvars_) {
            add_environment(ei.name.c_str(), ei.value.c_str());
        }
	    //创建描述符，前面已经分析过SocketInfo了。
        std::for_each(descriptors_.begin(), descriptors_.end(),
                      std::bind(&DescriptorInfo::CreateAndPublish, std::placeholders::_1, scon));

		...

        if (needs_console) {
            setsid();
            OpenConsole();
        } else {
            ZapStdio();
        }
		//设置parser函数解析好的参数
        SetProcessAttributes();

        //创建子进程
        if (!ExpandArgsAndExecve(args_)) {
            PLOG(ERROR) << "cannot execve('" << args_[0] << "')";
        }

        _exit(127);
    }
	
    if (oom_score_adjust_ != -1000) {
        std::string oom_str = std::to_string(oom_score_adjust_);
        std::string oom_file = StringPrintf("/proc/%d/oom_score_adj", pid);
        if (!WriteStringToFile(oom_str, oom_file)) {
            PLOG(ERROR) << "couldn't write oom_score_adj: " << strerror(errno);
        }
    }

    time_started_ = boot_clock::now();
    pid_ = pid;
    flags_ |= SVC_RUNNING;
    process_cgroup_empty_ = false;
	//不清楚什么鬼
    errno = -createProcessGroup(uid_, pid_);
        if (swappiness_ != -1) {
            if (!setProcessGroupSwappiness(uid_, pid_, swappiness_)) {
                PLOG(ERROR) << "setProcessGroupSwappiness failed";
            }
        }

        if (soft_limit_in_bytes_ != -1) {
            if (!setProcessGroupSoftLimit(uid_, pid_, soft_limit_in_bytes_)) {
                PLOG(ERROR) << "setProcessGroupSoftLimit failed";
            }
        }

        if (limit_in_bytes_ != -1) {
            if (!setProcessGroupLimit(uid_, pid_, limit_in_bytes_)) {
                PLOG(ERROR) << "setProcessGroupLimit failed";
            }
        }

    if ((flags_ & SVC_EXEC) != 0) {
        LOG(INFO) << "SVC_EXEC pid " << pid_ << " (uid " << uid_ << " gid " << gid_ << "+"
                  << supp_gids_.size() << " context "
                  << (!seclabel_.empty() ? seclabel_ : "default") << ") started; waiting...";
    }
    //更改状态
    NotifyStateChange("running");
    return true;
}

```





## Service 重启

系统中，有些进程需要一直存在。init就会再其stop后将其重新拉起。

首先，需要知道一点，子进程退出时，内核会给父进程发送`SIGCHLD`信号，所以init进程肯定是需要设置该信号的handler的。

```c++
// init.cpp & signal_handler.cpp
void signal_handler_init() {
    // Create a signalling mechanism for SIGCHLD.
    int s[2];
	// unix domain socket
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0, s) == -1) {
        PLOG(ERROR) << "socketpair failed";
        exit(1);
    }

    signal_write_fd = s[0];
    signal_read_fd = s[1];

    // Write to signal_write_fd if we catch SIGCHLD.
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = SIGCHLD_handler;
    act.sa_flags = SA_NOCLDSTOP;
    sigaction(SIGCHLD, &act, 0);
    
    ServiceManager::GetInstance().ReapAnyOutstandingChildren();
    register_epoll_handler(signal_read_fd, handle_signal);
}


static void SIGCHLD_handler(int) {
    if (TEMP_FAILURE_RETRY(write(signal_write_fd, "1", 1)) == -1) {
        PLOG(ERROR) << "write(signal_write_fd) failed";
    }
}
```

为什么不直接在信号handler中完成子进程重启的操作呢，可能因为signal是异步的，并且会中断进程的执行，所以需要尽可能的时间短？

从上面的代码可以知道，进程重启的逻辑主要是在`handle_signal`函数中。

```c++
static void handle_signal() {
    // Clear outstanding requests.
    char buf[32];
    read(signal_read_fd, buf, sizeof(buf));
	//处理子进程的SIGCHLD信号
    ServiceManager::GetInstance().ReapAnyOutstandingChildren();
}

bool ServiceManager::ReapOneProcess() {
    siginfo_t siginfo = {};
    	//获取一个已经终止的进程的信息。 注意 那个WNOWAIT标志。
        if (TEMP_FAILURE_RETRY(waitid(P_ALL, 0, &siginfo, WEXITED | WNOHANG | WNOWAIT)) != 0) {
            return false;
        }

    auto pid = siginfo.si_pid;
    if (pid == 0) return false;
	//C++ RAII，析构时执行 lambda表达式。用于清理僵尸进程。
    //因为waitid设置了 WNOWAIT，所以waitid并不会清空内核保存的进程信息，还需要再一次调用waitpid
    auto reaper = make_scope_guard([pid] { TEMP_FAILURE_RETRY(waitpid(pid, nullptr, WNOHANG)); });

    if (PropertyChildReap(pid)) {
        return true;
    }
	//找到子进程对应的Service
    Service* svc = FindServiceByPid(pid);

    std::string name;
    std::string wait_string;
    if (svc) {
        name = StringPrintf("Service '%s' (pid %d)", svc->name().c_str(), pid);
        if (svc->flags() & SVC_EXEC) {
            wait_string = StringPrintf(" waiting took %f seconds",
                                       exec_waiter_->duration().count() / 1000.0f);
        }
    } else {
        name = StringPrintf("Untracked pid %d", pid);
    }

    auto status = siginfo.si_status;
    if (WIFEXITED(status)) {
        LOG(INFO) << name << " exited with status " << WEXITSTATUS(status) << wait_string;
    } else if (WIFSIGNALED(status)) {
        LOG(INFO) << name << " killed by signal " << WTERMSIG(status) << wait_string;
    }

    if (!svc) {
        return true;
    }
	//重启进程。之歌函数仅仅是设置了flag位。
    svc->Reap();

    if (svc->flags() & SVC_EXEC) {
        exec_waiter_.reset();
    }
    if (svc->flags() & SVC_TEMPORARY) {
        RemoveService(*svc);
    }
    return true;
}
```

`svc->Reap`的主要工作就是设置Service的flag，清理descriptor以及执行`onrestart` action。

```c++
// Remove any descriptor resources we may have created.
std::for_each(descriptors_.begin(), descriptors_.end(),
	std::bind(&DescriptorInfo::Clean, std::placeholders::_1));

flags_ |= SVC_RESTARTING;

onrestart_.ExecuteAllCommands();
```

那么，问题来了，在哪完成实际的重启操作呢。

当然还是在init进程的loop里面：

```c++
if (!(waiting_for_prop || sm.IsWaitingForExec())) {
	if (!shutting_down) restart_processes();
}

static void restart_processes()
{
    process_needs_restart_at = 0;
    //找到flag 中含有SVC_RESTARTING位的service。并重启它。。。
    ServiceManager::GetInstance().ForEachServiceWithFlags(SVC_RESTARTING, [](Service* s) {
        s->RestartIfNeeded(&process_needs_restart_at);
    });
}

void Service::RestartIfNeeded(time_t* process_needs_restart_at) {
    boot_clock::time_point now = boot_clock::now();
    //进程中止5s后再尝试重新启动。
    boot_clock::time_point next_start = time_started_ + 5s;
    if (now > next_start) {
        flags_ &= (~SVC_RESTARTING);
        Start();
        return;
    }

    time_t next_start_time_t = time(nullptr) +
        time_t(std::chrono::duration_cast<std::chrono::seconds>(next_start - now).count());
    if (next_start_time_t < *process_needs_restart_at || *process_needs_restart_at == 0) {
        *process_needs_restart_at = next_start_time_t;
    }
}
```



## 总结

如果想知道如何对多进程进行管理，可以看看service的全部代码，也不多，千把行。看完以后，需要重新学习一下进程，进程组的知识了，全忘了。

笔记很杂，很乱，过段时间就忘了。。。。没事回头再看看，再修改。



## 附录

### Java如何使用init进程创建的socket

`fork`之后创建完`socket`，随后，就相应的`fd`以`ANDROID_SOCKET_SOCKNAME=fd`的形式添加到了环境变量中。

```c++
// #system/core/init/service.cpp
int pid = fork();
if (pid == 0) {
    std::for_each(descriptors_.begin(), descriptors_.end(),
                      std::bind(&DescriptorInfo::CreateAndPublish, std::placeholders::_1, scon));
}

// #system/core/init/descriptors.cpp
void DescriptorInfo::CreateAndPublish(const std::string& globalContext) const {
  // Create
  const std::string& contextStr = context_.empty() ? globalContext : context_;
  int fd = Create(contextStr);
  if (fd < 0) return;

  // Publish
  std::string publishedName = key() + name_;
  std::for_each(publishedName.begin(), publishedName.end(),
                [] (char& c) { c = isalnum(c) ? c : '_'; });

  std::string val = std::to_string(fd);
  //添加到环境变量中。
  add_environment(publishedName.c_str(), val.c_str());

  // make sure we don't close on exec
  fcntl(fd, F_SETFD, 0);
}
```

以`Zygote`为例：

```java
public static void main(String argv[]) {
    ZygoteServer zygoteServer = new ZygoteServer();
	String socketName = "zygote";
    zygoteServer.registerServerSocket(socketName);
}

void registerServerSocket(String socketName) {
    if (mServerSocket == null) {
        int fileDesc;
        final String fullSocketName = ANDROID_SOCKET_PREFIX + socketName;
        try {
            //从环境变量中拿到env
            String env = System.getenv(fullSocketName);
            fileDesc = Integer.parseInt(env);
        } catch (RuntimeException ex) {
            throw new RuntimeException(fullSocketName + " unset or invalid", ex);
         }

        try {
            FileDescriptor fd = new FileDescriptor();
            fd.setInt$(fileDesc);
            mServerSocket = new LocalServerSocket(fd);
        } catch (IOException ex) {
            throw new RuntimeException(
                    "Error binding to local socket '" + fileDesc + "'", ex);
        }
    }
}
```

