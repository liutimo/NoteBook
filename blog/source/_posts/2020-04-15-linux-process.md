---
title: linux process
date: 2020-04-15 16:42:42
categories: linux
tags: 
    - 进程
---



## uid、effective-uid 及 set-uid

`uid`通常表示创建进程的用户

`euid`通常表示进程对于文件或资源的访问权限。

同理，`gid`表示创建进程的用户所在的用户组。`egid`表示当前进程对文件或资源的访问权限同`egid`对应的进程组是一样的。

```c++
auto uid    = getuid();
auto euid   = geteuid();
auto gid    = getgid();
auto egid   = getegid();
```

普通用户执行结果:

```shell
 uid:1000
euid:1000
 gid:1000
egid:1000
```

root用户执行结果

```shell
 uid:0
euid:0
 gid:0
egid:0
```



设置set-user-id 及 set-group-id

```shell
chmod u+s Uid 
chmod g+s Uid
```

root用户执行

```shell
 uid:0
euid:1000
 gid:0
egid:1000
```

root用户执行时，euid和egid都是1000。说明，执行程序在设置set-id位后，其他用户运行时，会拥有执行程序所属用户及用户组相应的权限。

实例

```shell
-rwsrwxr-x  1 root root 32736 Apr 25 21:36 Uid
-r--r-----  1 root root     0 Apr 25 21:35 1.txt
```

普通用户通过程序`Uid`打开文件`1.txt`。因为`Uid`设置了`set-user-id`位，所以能够打开`1.txt`

> 当我们以写权限打开1.txt时，如果Uid设置了set-user-id位，我们还是能够写入数据的1.txt。
>
> 有些文件设置了只读，一般是不是修改文件的，但是如果你是文件的owner或者root的话，通过wq!还是能保存文件退出。所以，open系统调用内部，这些规则应该是open函数内部定下的。




## wait族函数

```C++
#include <sys/types.h>
#include <sys/wait.h>

pid_t wait(int *status);
pid_t waitpid(pid_t pid, int *status, int options);
int waitid(idtype_t idtype, id_t id, siginfo_t *infop, int options);
```

这三个函数用于等待子进程的状态改变，并且返回子进程的状态改变信息。

> A state change is considered to be: the child terminated; the child was stopped by a signal; or the child was resumed by a signal. 

当子进程Term时，如果不执行`wait`操作，那么子进程就会成为zombie进程。

```c++
wait(&status) == waitpid(-1, &status, 0);	
```

 

前两个用到了再说，这里直说第三个`waitid`。

```c++
bool ServiceManager::ReapOneProcess() {
    siginfo_t siginfo = {};
    if (TEMP_FAILURE_RETRY(waitid(P_ALL, 0, &siginfo, WEXITED | WNOHANG | WNOWAIT)) != 0) {
        PLOG(ERROR) << "waitid failed";
        return false;
    }
    ...
}

int waitid(idtype_t idtype, id_t id, siginfo_t *infop, int options);
```

waitid比其他wait函数提供更细度的控制

1. `idtype` 和 `id`
    | `idtype` | desc                           |
    | -------- | ------------------------------ |
    | `P_PID`  | 等待进程号为`id`的子进程       |
    | `P_PGID` | 等待进程组ID为`id`的所有子进程 |
    | `P_ALL`  | 忽略`id`。等待所有子进程       |

2. `options`

    指定需要接收的进程状态。
    
    | `options`    | `desc`                                                       |
| ------------ | ------------------------------------------------------------ |
    | `WEXITED`    | 进程terminated                                               |
    | `WSTOPPED`   | 进程被信号stop。暂停？                                       |
    | `WCONTINUED` | 进程恢复运行，通过信号`SIGCONT`                              |
    | `WNOHANG`    | 没有子进程结束运行就立即返回                                 |
    | `WNOWAIT`    | 啊哈，实际的结果就是，添加该选项后，子进程仍然是僵尸进程，wait族函数再次调用还是可以取到该进程的信息 |
    
3. `siginfo_t`

    ```c++
    typedef struct
      {
        int si_signo;		/* Signal number.  */
        int si_errno;		/* If non-zero, an errno value associated with
    				   this signal, as defined in <errno.h>.  */
        int si_code;		/* Signal code.  */
    
        union
          {
    	int _pad[__SI_PAD_SIZE];
    
    	 /* kill().  */
    	struct
    	  {
    	    __pid_t si_pid;	/* Sending process ID.  */
    	    __uid_t si_uid;	/* Real user ID of sending process.  */
    	  } _kill;
    
    	/* POSIX.1b timers.  */
    	struct
    	  {
    	    int si_tid;		/* Timer ID.  */
    	    int si_overrun;	/* Overrun count.  */
    	    sigval_t si_sigval;	/* Signal value.  */
    	  } _timer;
    
    	/* POSIX.1b signals.  */
    	struct
    	  {
    	    __pid_t si_pid;	/* Sending process ID.  */
    	    __uid_t si_uid;	/* Real user ID of sending process.  */
    	    sigval_t si_sigval;	/* Signal value.  */
    	  } _rt;
    
    	/* SIGCHLD.  */
    	struct
    	  {
    	    __pid_t si_pid;	/* Which child.  */
    	    __uid_t si_uid;	/* Real user ID of sending process.  */
    	    int si_status;	/* Exit value or signal.  */
    	    __sigchld_clock_t si_utime;
    	    __sigchld_clock_t si_stime;
    	  } _sigchld;
    
    	/* SIGILL, SIGFPE, SIGSEGV, SIGBUS.  */
    	struct
    	  {
    	    void *si_addr;	/* Faulting insn/memory ref.  */
    	    short int si_addr_lsb;	/* Valid LSB of the reported address.  */
    	    struct
    	      {
    		void *_lower;
    		void *_upper;
    	      } si_addr_bnd;
    	  } _sigfault;
    
    	/* SIGPOLL.  */
    	struct
    	  {
    	    long int si_band;	/* Band event for SIGPOLL.  */
    	    int si_fd;
    	  } _sigpoll;
    
    	/* SIGSYS.  */
    	struct
    	  {
    	    void *_call_addr;	/* Calling user insn.  */
    	    int _syscall;	/* Triggering system call number.  */
    	    unsigned int _arch; /* AUDIT_ARCH_* of syscall.  */
    	  } _sigsys;
          } _sifields;
      } siginfo_t __SI_ALIGNMENT;
    
    
    /* X/Open requires some more fields with fixed names.  */
    # define si_pid		_sifields._kill.si_pid
    # define si_uid		_sifields._kill.si_uid
    # define si_timerid	_sifields._timer.si_tid
    # define si_overrun	_sifields._timer.si_overrun
    # define si_status	_sifields._sigchld.si_status
    # define si_utime	_sifields._sigchld.si_utime
    # define si_stime	_sifields._sigchld.si_stime
    # define si_value	_sifields._rt.si_sigval
    # define si_int		_sifields._rt.si_sigval.sival_int
    # define si_ptr		_sifields._rt.si_sigval.sival_ptr
    # define si_addr	_sifields._sigfault.si_addr
    # define si_addr_lsb	_sifields._sigfault.si_addr_lsb
    # define si_lower	_sifields._sigfault.si_addr_bnd._lower
    # define si_upper	_sifields._sigfault.si_addr_bnd._upper
    # define si_band	_sifields._sigpoll.si_band
    # define si_fd		_sifields._sigpoll.si_fd
    # define si_call_addr 	_sifields._sigsys._call_addr
    # define si_syscall	_sifields._sigsys._syscall
    # define si_arch	_sifields._sigsys._arch
    ```


## 进程组

```c++

```

