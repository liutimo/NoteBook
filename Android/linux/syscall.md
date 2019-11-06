## 系统调用函数



### `函数名`

#### 函数原型

#### 函数描述

#### 实例









### `dup族函数`

#### 函数原型

```c
#include <unistd.h>
int dup(int oldfd);
int dup2(int oldfd, int newfd);

#define _GNU_SOURCE             /* See feature_test_macros(7) */
#include <fcntl.h>              /* Obtain O_* constant definitions */
#include <unistd.h>
int dup3(int oldfd, int newfd, int flags);

```



#### 函数描述

1. `dup` 

    创建oldfd的副本。返回值为当前最小的未使用的描述符。newfd 和 oldfd 共享 file offest 和 file status flags。不共享 file descriptor flags（the close-on-exec flag，即FD_CLOEXEC）。 

    > File Status Flags
    >
    > 1. 文件访问性状态标志
    >
    >     `O_RDONLY`;`O_WRONLY`;`O_RDWR`
    >
    > 2. 文件创建性状态标志
    >
    >     `O_CREAT`;`O_EXCL`; `O_NOCTTY`;`O_TRUNC`;`O_APPEND`
    >
    > 3. 文件异步/同步标志
    >
    >     `O_NONBLOCK` ; `O_SYNC`; `O_DSYNC` ;` O_RSYNC`;`O_FSYNC`; `O_ASYNC`

#### 实例





### `epoll`

#### 函数原型

```c
#include <sys/epoll.h>
int epoll_create(int size);
int epoll_create1(int flags);

int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);

int epoll_wait(int epfd, struct epoll_event *events,
                      int maxevents, int timeout);
int epoll_pwait(int epfd, struct epoll_event *events,
                      int maxevents, int timeout,
                      const sigset_t *sigmask);
```



#### 函数描述

#### 实例







### `mount`

#### 函数原型

```c
#include <sys/mount.h>
int mount(const char *source, const char *target, 
          const char *filesystemtype, 
          unsigned long mountflags, const void *data);
```

#### 函数描述

- `source`:

    将要挂载的文件系统

- `target`

    文件系统所要挂载的目标目录

- `filesystemtype`

    文件系统的类型。通过命令`cat /proc/filesystems` 可查看操作系统支持的文件系统类型

- `mountflags`

    4字节. top 16bits 为magic number (0xC0ED), low order 16 bits 为实际的mountflags.	

    | flag             | 描述 |
    | ---------------- | ---- |
    | `MS_BIND`        |      |
    | `MS_DIRSYNC`     |      |
    | `MS_LAZYTIME`    |      |
    | `MS_MANDLOCK`    |      |
    | `MS_MOVE`        |      |
    | `MS_NOATIME`     |      |
    | `MS_NODEV`       |      |
    | `MS_NODIRATIME`  |      |
    | `MS_NOEXEC`      |      |
    | `MS_NOSUID`      |      |
    | `MS_RDONLY`      |      |
    | `MS_RELATIME`    |      |
    | `MS_REMOUNT`     |      |
    | `MS_SILENT`      |      |
    | `MS_STRICTATIME` |      |
    | `MS_SYNCHRONOUS` |      |
    | `MS_NODIRATIME`  |      |
    |                  |      |

- `data`

    不同的文件系统对应的解析方式不一致。`man 8 mount `

#### 实例

将U盘挂载到 ./sdcard。

```c
#include <sys/mount.h>
#include <iostream>
#include <errno.h>
int main()
{
	std::cout << mount("/dev/sdb1", "sdcard", "vfat", 0, "mode=0755") << std::endl;
	std::cout << errno << std::endl;
	return 0;
}
```

