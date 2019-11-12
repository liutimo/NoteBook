# Sokcet IO

## 发送数据

### 相关函数

```c
#include <sys/types.h>
#include <sys/socket.h>
ssize_t send(int sockfd, const void *buf, size_t len, int flags);
ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
					const struct sockaddr *dest_addr, socklen_t addrlen);
ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags);
```

​	这些函数都用于发送网络数据。

- `sockfd`

    发送socket的描述符

- `buf` 和`len` 

    待发送数据及其长度

- `flags`
	

| flag            | desc |
	| --------------- | ---- |
	| `MSG_CONFIRM`   |      |
	| `MSG_DONTROUTE` |      |
	| `MSG_DONTWAIT`  |      |
	| `MSG_EOR`       |      |
	| `MSG_MORE`      |      |
	| `MSG_NOSIGNAL`  |      |
	| `MSG_OOB`       |      |
	|                 |      |
	
	
	
- `dest_addr` 和`addrlen`

    接收数据的target的地址信息及其长度

- `msg`

    [参考链接](https://linux.die.net/man/2/recv)
    
    ```c
    struct msghdr {
        void         *msg_name;       /* optional address */
        socklen_t     msg_namelen;    /* size of address */
        struct iovec *msg_iov;        /* scatter/gather array */
        size_t        msg_iovlen;     /* # elements in msg_iov */
        void         *msg_control;    /* ancillary data, see below */
        size_t        msg_controllen; /* ancillary data buffer len */
        int           msg_flags;      /* flags on received message */
    };
    ```
    
     1. `msg_name` 
    
        由第二个参数的类型`socklen_t`可以大致了解，`msg_name`的类型可能是`sockaddr_in` or `sockaddr`。即该参数用于表示地址信息，当用于Netlink时，其类型为`sockaddr_ln`。
    
     2. `msg_iov`
    
        linux高级IO，可用于发送一次发送多个不连续的缓冲区。
        
    3. `msg_control`
    
        用于发送控制信息，其buffer最大长度可通过如下命令查看
    
        `cat /proc/sys/net/core/optmem_max`
    
    4. `msg_flags`
    
        

#### `send`

用于处于连接状态的socket，因此预期的数据接收者是已知的。`send`和`write`的区别在于`flags`参数。

`send(sockfd, buf, len, 0)` == `write(sockfd, buf, len)`。

同时，`send(sockfd, buf, len, flags)` == `sendto(sockfd, buf, len, flags, NULL, 0)`

#### `sendto`

1. 用于connection-mode(`SOCK_STREAM` or `SOCKE_SEQPACKET`)时，`dest_addr`和`addrlen`参数需要被忽略(设置为`NULL`和 0)，否则，返回错误码`EISCONN`。
2. 用于无连接模式（UDP)。

#### `sendmsg`

和`send`及`sendto`不同，`sendmsg`发送的数据存储在`msg.msg_iov`中。同时允许发送额外的数据(通常是控制信息)。



### 注意

当发送的数据过长导致无法atomically传输到底层协议时，返回`EMSGSIZE`，并且数据不会被发送出去。

通常是`sendto`。



