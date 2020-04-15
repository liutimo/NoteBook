---
title: linux process
date: 2020-04-15 16:42:42
categories: linux
tags: 
    - 进程
---

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

    