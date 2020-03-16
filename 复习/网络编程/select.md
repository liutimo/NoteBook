## fd_set

```c++
typedef long int __fd_mask;
#define __FD_SETSIZE		1024
#define __NFDBITS	(8 * (int) sizeof (__fd_mask))
typedef struct {
	__fd_mask fds_bits[__FD_SETSIZE / __NFDBITS];
} fd_set;
```

linux默认select所支持的最大的fd的编号是 __FD_SETSIZE。

> 进程的fd分配原则是，当前可用最小的fd， 所以理论上，select可最大同事支持 FD_SETSIZE - 3个fd。
>
> 减去的三个对应标准 输入、输出、错误。

fd_set所占用的内存，每bit表示一个fd。那么

1024bit / 8 = 128 byte.

所以fd_set结构体的大小是128byte。



### FD_SET的内存结构

连续的1024bit数组。当FD_SET(100, &rfds);执行后，rfds对应的第100bit会被置1。



### 突破FD_SETSIZE的限制

1.  在include 前重新定义FD_SETSIZE
2. 使用动态内存。参考libevent中的selectop的实现。



