# 为什么使用`readv()`和`writev()`

因为使用`read()`将数据读到不连续的内存、使用`write()`将不连续的内存发送出去，要经过多次的调用`read`、`write`
如果要从文件中读一片连续的数据至进程的不同区域，有两种方案：①使用read()一次将它们读至一个较大的缓冲区中，然后将它们分成若干部分复制到不同的区域； ②调用read()若干次分批将它们读至不同区域。 同样，如果想将程序中不同区域的数据块连续地写至文件，也必须进行类似的处理。



UNIX提供了另外两个函数—`readv()`和`writev()`，它们只需一次系统调用就可以实现在文件和进程的多个缓冲区之间传送数据，免除了多次系统调用或复制数据的开销。



# 函数原型

```c
#include <sys/uio.h>
struct iovec {
    void  *iov_base;    /* Starting address */
    size_t iov_len;     /* Number of bytes to transfer */
};
ssize_t readv(int fd, const struct iovec *iov, int iovcnt);
ssize_t writev(int fd, const struct iovec *iov, int iovcnt);
```
- `fd`
  文件描述符

- `iov`

    指向`iovec`结构体数组首地址的指针咯

- `iovcnt`

    `iov`对应的数组长度咯

- 返回值

    读写的总字节数。 -1表示失败咯





# 示例

```c

```

