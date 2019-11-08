[参考博客](https://blog.csdn.net/u012819339/article/details/51334600)



# Netlink

用于kernel 和 user sapce 之间的通信。Netlink是面向报文的，类似于UDP。

> 优点：
>
> 1. 双向传输，支持异步
> 2. user space使用标准socket API
> 3. 支持多播
> 4. 可由kernel发起通信
> 5. 支持32中协议类型。（有时可能不够用）





# 函数原型

## user space



### 函数说明

```c
#include <asm/types.h>
#include <sys/socket.h>
#include <linux/netlink.h>

netlink_socket = socket(AF_NETLINK, socket_type, netlink_family);
```

 - `socket_type`

    取值可以是`SOCK_RAW `和`SOCK_DGRAM`。但是netlink协议并不区分这两者之间的区别。

- `netlink_family `
A netlink family usually specifies more message types

  | value               | desc                                                         |
  | ------------------- | ------------------------------------------------------------ |
  | `NETLINK_ROUTE`     | Receives routing and link updates and may be used to modify  the routing tables (both IPv4 and IPv6), IP addresses, link  parameters, neighbor setups, queueing disciplines, traffic               classes and packet classifiers |
  | `NETLINK_NFLOG`     | Netfilter/iptables ULOG.                                     |
  | `NETLINK_NETFILTER` | Netfilter subsystem                                          |
  |                     |                                                              |

    ### 相关结构体

    1. `struct nlmsghdr`

        ```c
        struct nlmsghdr {
        	__u32 nlmsg_len;    /* Length of message including header */
        	__u16 nlmsg_type;   /* Type of message content */
        	__u16 nlmsg_flags;  /* Additional flags */
        	__u32 nlmsg_seq;    /* Sequence number */
        	__u32 nlmsg_pid;    /* Sender port ID */
        };
        ```

        

        - `nlmsg_type`

            消息类型

            | type          | desc                                                         |
            | ------------- | ------------------------------------------------------------ |
            | `NLMSG_NOOP`  | message is to be ignored                                     |
            | `NLMSG_ERROR` | message signals an error and the payload contains an nlmsgerr structure |
            | `NLMSG_DONE`  | message terminates   a multipart message                     |
            |               |                                                              |

            ```c
            struct nlmsgerr {
            	int error;        /* Negative errno or 0 for acknowledgements */
            	struct nlmsghdr msg;  /* Message header that caused the error */
            };
            ```