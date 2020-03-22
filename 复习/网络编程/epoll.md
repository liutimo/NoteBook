# 简介

epoll是poll的变种，支持ET和LT两种接口，并且对于 large numbers of watched file descriptors 有较好的伸缩性。

# epoll API

epoll API

## `epoll_create`

```c++
#include <sys/epoll.h>
//now, size is unused. but muse be greater than 0.
//内核会根据epoll_ctl 动态的调整数据结构的size
int epoll_create(int size);
//flags == 0: is same as epoll_create
// EPOLL_CLOEXEC  set close-on-exec flag 
int epoll_create1(int flags);

//On success, these system calls return a nonnegative file descriptor.  On error, -1 is returned, and errno is set to indicate the error.
```

创建一个epoll “实例”。



## `epoll_ctl`

```c++
#include <sys/epoll.h>
//When successful, epoll_ctl() returns zero.  When an error occurs, epoll_ctl() returns -1 and errno is set appropriately.
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
```

- `op`

    | op              | 含义              |
    | --------------- | ----------------- |
    | `EPOLL_CTL_ADD` | 添加event到EPOLL  |
    | `EPOLL_CTL_MOD` | 修改已添加的event |
    | `EPOLL_CTL_DEL` | 删除已添加的event |

- `event`

    ```c++
    typedef union epoll_data {
    	void        *ptr;
    	int          fd;
    	uint32_t     u32;
    	uint64_t     u64;
    } epoll_data_t;
    
    struct epoll_event {
    	uint32_t     events;      /* Epoll events */
    	epoll_data_t data;        /* User data variable */
    };
    ```

    | events         | 含义                                                         |
    | -------------- | ------------------------------------------------------------ |
    | `EPOLLIN`      | read                                                         |
    | `EPOLLOUT`     | write                                                        |
    | `EPOLLRDHUP`   | 对端连接关闭。stream socket                                  |
    | `EPOLLPRI`     | 带外数据                                                     |
    | `EPOLLERR`     | error                                                        |
    | `EPOLLHUP`     | hang up                                                      |
    | `EPOLLET`      | ET模式，默认LT                                               |
    | `EPOLLONESHOT` | oneshot，当通过epoll_wait返回后，需要再次调用epoll_ctl来操作才能被处罚 |



## `epoll_wait`

```c++
#include <sys/epoll.h>

//maxevents muse be greater than 0.
//timeout: maximum time of ${timeout} milliseconds
int epoll_wait(int epfd, struct epoll_event *events,
               int maxevents, int timeout);
int epoll_pwait(int epfd, struct epoll_event *events,
                int maxevents, int timeout,
                const sigset_t *sigmask);

//When  successful, epoll_wait() returns the number of file descriptors ready for the requested I/O, or zero if no file descriptor became ready during the requested timeout milliseconds.  When an error occurs, epoll_wait() returns -1 and errno is set appropriately.
```

这两个函数都用于等待fd的event。区别在后者可以通过设置sigset_t来暂时阻塞指定信号，避免函数执行被信号打断。

```c++
//The following epoll_pwait() call:
ready = epoll_pwait(epfd, &events, maxevents, timeout, &sigmask);
//is equivalent to atomically executing the following calls:
sigset_t origmask;
sigprocmask(SIG_SETMASK, &sigmask, &origmask);
ready = epoll_wait(epfd, &events, maxevents, timeout);
sigprocmask(SIG_SETMASK, &origmask, NULL);

```









# ET和LT的区别 及 如何保证数据被完整读取







# libevent中epoll的使用细节

1. 对于`evconnlistener`, listen fd 是采用`EV_READ|EV_PERSIST`。

   >  原因应该是，当两个connection一起到来时，如果使用ET模式，可能会导致第二个conntion可能无法被accept？？？

2. 



# epoll 的实现细节