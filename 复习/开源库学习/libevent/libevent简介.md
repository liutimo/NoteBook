# libevent简介



libevent提供一系列的select替代接口，并且使用当前操作系统所具有的最高效的版本。其支持多种不同的IO复用方式，包括`poll`,`epoll`,`kqueue`, `evports`和`/dev/poll`。libevent通过对这些接口进行封装，

libevent是一个轻量级的高性能事件通知库。

当事件触发(事件包括：文件描述符可读写、定时器等)时，其会调用相应事件对应的回调函数对事件进行处理。

libevent的优点如下：

1. 事件驱动，高性能
2. 支持I\O、定时器以及信号等事件
3. 源码精炼，易读
4. 支持多种IO多路复用技术：select、epoll、poll等

目前libevent已经被广泛应用，作为底层的网络库。其中包括memcached。

# libevent使用



## IO事件

对于IO包括网络IO、串口或者是管道之类的都可以支持。

相关函数如下:

```c++
//创建一个event，fd表示文件描述符，与event_free成对使用
struct event *event_new(struct event_base *base, evutil_socket_t fd, short events, 						event_callback_fn callback, void *callback_arg);
//给一个allocated 的event重新设置相应参数
int event_assign(struct event *ev, struct event_base *base, evutil_socket_t fd, short events, 				event_callback_fn callback, void *callback_arg);
//释放event_new创建的event的memory
void event_free(struct event *);
//添加到事件循环
int event_add(struct event *ev, const struct timeval *timeout);
//从事件循环中删除
int event_del(struct event *ev);
```



以一个简单的echo server来说明这些函数的用法:

```c++

```













## 信号







## 定时事件

定时器相关的函数如下:

```C++
#define evtimer_assign(ev, b, cb, arg) \
	event_assign((ev), (b), -1, 0, (cb), (arg))

//初始化一个定时事件
#define evtimer_new(b, cb, arg)		event_new((b), -1, 0, (cb), (arg))
//添加到事件looper中
#define evtimer_add(ev, tv)		event_add((ev), (tv))
//将事件从looper中删除
#define evtimer_del(ev)			event_del(ev)
//判断事件是pending 还是 scheduled
#define evtimer_pending(ev, tv)		event_pending((ev), EV_TIMEOUT, (tv))
//判断事件是否初始化
#define evtimer_initialized(ev)		event_initialized(ev)
```

首先看一下`evtimer_new`的定义，其仅仅是给`event_new`函数设置了两个默认参数(fd 默认为-1， event type 默认为 0)。为什么这样做呢？我们先看一个使用定时器的例子。

```c++
#include <iostream>
#include "util.h"

#include <event2/event.h>
#include <signal.h>

static struct timeval lasttime;

static void timeout_callback(evutil_socket_t fd, short event, void *arg) {
    printf("fd %d   event %d", fd, event);
    struct timeval newtime, diffence;
    struct event *ev = (struct event *)arg;

    evutil_gettimeofday(&newtime, nullptr);
    evutil_timersub(&newtime, &lasttime, &diffence);
    auto elapsed = diffence.tv_sec + diffence.tv_usec / 1.0e6;

    print(std::to_string(elapsed));

    lasttime = newtime;

    //重新设置定时任务
    struct timeval tv;
    evutil_timerclear(&tv);
    tv.tv_sec = 2;
    event_add(ev, &tv);
}

static void signal_callback(evutil_socket_t fd, short event, void *arg) {
    struct event_base *base = (struct event_base*)(arg);
    print("exit...");
    event_base_loopbreak(base);
}

int main(int argc, char *argv[]) {
    print(argv[0]);
    struct event_base *base = event_base_new();
    assert(base != nullptr);

    //用于退出事件循环
    struct event *signal_ev = evsignal_new(base, SIGINT, signal_callback, base);
    evsignal_add(signal_ev, nullptr);

    struct timeval tv;
    evutil_timerclear(&tv);
    tv.tv_sec = 2;

    struct event *timeout_ev = evtimer_new(base, timeout_callback, event_self_cbarg());
	//[1]
    //struct event *timer_ev = event_new(base, 100, 100, timeout_callback, event_self_cbarg());
    
    evtimer_add(timeout_ev, &tv);

    evutil_gettimeofday(&lasttime, nullptr);

    event_base_dispatch(base);

    event_base_free(base);
    return 0;
}
```

在上面这个例子中，定时器事件每2s触发一次。对于定时器事件而言，`evtimer_new`并没有设置相关的时间，时间是通过`evtimer_add`的第二个参数设置的。仔细想想，`evtimer_new`的定义中`event_new`的两个默认参数难道无实际意义？

将**[1]**处的代码取消注释，发现报如下错误:

```shell
[warn] Epoll ADD(-2147483644) on fd 100 failed. Old events were 0; read change was 0 (none); write change was 33 (add); close change was 0 (none): Bad file descriptor
```

恩，大概有了了解，`event_add`通过判断`event->fd`是否大于0来判断是否需要添加到`epoll`中(linux)。





