# evbuffer

> evbuffer是`libevent`为了实现向后添加数据和从前面移除数据而优化的队列。

其相关数据结构和函数定义



## 获取`evbuffer`中的数据长度。

```c++
size_t evbuffer_get_length(const struct evbuffer *buf); //[1]
size_t evbuffer_get_contiguous_space(const struct evbuffer *buf); //[2]
```

- `evbuffer_get_length`

​	获取整个`evbuffer`中保存的字节数(`evbuffer->total_len`)。

- `evbuffer_get_contiguous_space`

  获取`evbuffer`中第一个chain的字节数。(`evbuffer->first->off`)

> `evbuffer`中数据不一定是保存在一个连续的内存中的，其可能保存在多个不连续的内存块中,在代码中用chain表示。

## 将`evbuffer`中的数据线性化

将`evbuffer`中的前`size`个字节的数据线性化，必要时将进行复制或移动，以保证这些字节是连续的。

```c++
unsigned char *evbuffer_pullup(struct evbuffer *buf, ev_ssize_t size);
```

`size < 0` : 线性化整个`evbuffer`

`size > total_len` : 返回`NULL`



## 向`evbuffer`的尾部添加数据

该函数向`evbuffer`的尾部添加数据。成功返回0，失败返回-1。

```c++
int evbuffer_add(struct evbuffer *buf, const void *data, size_t datlen);
```

另外，还有两个辅助函数可用于添加格式化的数据。

```c++
int evbuffer_add_printf(struct evbuffer *buf, const char *fmt, ...);
int evbuffer_add_vprintf(struct evbuffer *buf, const char *fmt, va_list ap)
```

当内部不足以添加`datalen`字节的数据时， 会调动`evbuffer_expand`扩展`evbuffer`的内存空间。

```c++
//成功返回0，失败返回-1。
int evbuffer_expand(struct evbuffer *buf, size_t datlen);
```

该函数修改缓冲区中的最后一个chain，或者添加一个新的chain，使得缓冲区能容纳`datalen`个字节的数据。

## 从一个`evbuffer`向另一个`evbuffer`移动数据

**移动到尾部**

```c++
int evbuffer_add_buffer(struct evbuffer *outbuf, struct evbuffer *inbuf);
int evbuffer_remove_buffer(struct evbuffer *src, struct evbuffer *dst, size_t datlen);
```

- `evbuffer_add_buffer`

  将`outbuf`中的所有数据移动到`inbuf`中。

- `evbuffer_remove_buffer`

  将`src`中移动`datalen`字节的数据到`dst`的末尾，尽量可能避免移动。

**移动到首部**

```C++
int evbuffer_prepend(struct evbuffer *buf, const void *data, size_t size);
int evbuffer_prepend_buffer(struct evbuffer *dst, struct evbuffer* src);
```

- `evbuffer_prepend` 

    将`data`中的数据加入到`buf`的前面

 - `evbuffer_prepend_buffer`

    将`src`中的所有数据移动到`dst`的前面。

## 从`evbuffer`中删除数据

```c++
int evbuffer_drain(struct evbuffer *buf, size_t len);
int evbuffer_remove(struct evbuffer *buf, void *data, size_t datlen);
```

- `evbuffer_remove`

  从`buf`中移除并复制`datalen`字节到`data`中。

- `evbuffer_drain`

  行为和`evbuffer_remove`类似，但是不会复制数据。

## 从`evbuffer`中拷贝数据

有时候需要获取缓冲区前面数据的副本，而不清除数据。比如说，可能需要查看某特定类型的记录是否已经完整到达，而不清除任何数据（像`evbuffer_remove`那样），或者在内部重新排列缓冲区（像`evbuffer_pullup`那样）。

```c++
ev_ssize_t evbuffer_copyout(struct evbuffer *buf, void *data_out, size_t datlen);
ev_ssize_t evbuffer_copyout_from(struct evbuffer *buf, const struct evbuffer_ptr *pos, void *data_out, size_t datlen);
```

`evbuffer_copyout()`的行为与`evbuffer_remove()` 类似，但是它不从缓冲区移除任何数据。也就是说，它从`buf`前面复制`datlen`字节到`data`处的内存中。如果可用字节少于`datlen`，函数会复制所有字节。失败时返回-1，否则返回复制的字节数。
`evbuffer_copyout_from()`的行为与`evbuffer_copyout()`有些类似， 不同的是`evbuffer_copyout_from()` 从制定的位置`pos` 开始复制， 而不是`buffer`的起始位置。 

## 面向行的输入

很多协议中使用的是基于行的格式。`evbuffer_readln`从`evbuffer`前面取出一行。

```c++
enum evbuffer_eol_style {
        EVBUFFER_EOL_ANY,		//行尾是任意数量、任意次序的回车和换行符
        EVBUFFER_EOL_CRLF,		// \r\n 或者 \n
        EVBUFFER_EOL_CRLF_STRICT,	// \r\n 
        EVBUFFER_EOL_LF,		// \n
        EVBUFFER_EOL_NUL		// \0
};
char *evbuffer_readln(struct evbuffer *buffer, size_t *n_read_out,
    enum evbuffer_eol_style eol_style);
```

## 在`evbuffer`中搜索

```c++
struct evbuffer_ptr {
	ev_ssize_t pos;
	struct {
		void *chain;
		size_t pos_in_chain;
	} internal_;  //由库内部使用，用户代码不应该访问
};
```

`evbuffer_ptr`指示`evbuffer`中的一个位置，主要用于在`evbuffer`中搜索数据。

```c++
struct evbuffer_ptr evbuffer_search(struct evbuffer *buffer,
    const char *what, size_t len, const struct evbuffer_ptr *start);
struct evbuffer_ptr evbuffer_search_range(struct evbuffer *buffer,
    const char *what, size_t len, const struct evbuffer_ptr *start,
    const struct evbuffer_ptr *end);
struct evbuffer_ptr evbuffer_search_eol(struct evbuffer *buffer,
    struct evbuffer_ptr *start, size_t *eol_len_out,
    enum evbuffer_eol_style eol_style);
```

`evbuffer_search`，在缓冲区中搜索字符串，`start`指定开始搜索的位置。

`evbuffer_search_range`,和`evbuffer_search`类似，但是指定了搜索的结束位置。

`evbuffer_search_eol`函数像`evbuffer_readln()`一样检测行结束，但是不复制行，而是返回指向行结束符的`evbuffer_ptr`。如果`eol_len_out`非空，则它被设置为`EOL`字符串长度。

```C++
enum evbuffer_ptr_how {
        EVBUFFER_PTR_SET,
        EVBUFFER_PTR_ADD
};
int evbuffer_ptr_set(struct evbuffer *buffer, struct evbuffer_ptr *pos,
    size_t position, enum evbuffer_ptr_how how);
```

`evbuffer_ptr_set()`函数操作`buffer`中的位置`pos`。如果`how`等于`EVBUFFER_PTR_SET`,指针被移动到缓冲区中的绝对位置`position`；如果等于`EVBUFFER_PTR_ADD`，则向前移动`position`字节。函数成功时返回0，失败时返回-1。

> 任何修改`evbuffer`或者其布局的调用都会使得`evbuffer_ptr`失效，不能再安全地使用。



## 检测数据而不复制

有时候需要读取`evbuffer`中的数据而不进行（像`evbuffer_copyout()`需要进行内存拷贝）复制，也不重新排列内部内存布局（像`evbuffer_pullup`()那样）。有时候可能需要查看`evbuffer`中间的数据。

```c++
struct evbuffer_iovec {
        void *iov_base;
        size_t iov_len;
};

int evbuffer_peek(struct evbuffer *buffer, ev_ssize_t len,
    struct evbuffer_ptr *start_at,
    struct evbuffer_iovec *vec_out, int n_vec);
```

> - 修改`evbuffer_iovec`所指的数据会导致不确定的行为
> - 如果任何函数修改了`evbuffer`，则`evbuffer_peek()`返回的指针会失效
> - 如果在多个线程中使用`evbuffer`，确保在调用`evbuffer_peek()`之前使用`evbuffer_lock()`，在使用完`evbuffer_peek()`给出的内容之后进行解锁

## 使用evbuffer的网络IO

libevent中`evbuffer`的最常见使用场合是网络IO。将`evbuffer`用于网络IO的接口是：

```
int evbuffer_write(struct evbuffer *buffer, evutil_socket_t fd);
int evbuffer_write_atmost(struct evbuffer *buffer, evutil_socket_t fd,
        ev_ssize_t howmuch);
int evbuffer_read(struct evbuffer *buffer, evutil_socket_t fd, int howmuch);1234
```

`evbuffer_read()`函数从套接字`fd`读取至多`howmuch`字节到`buffer`末尾。函数成功时返回读取的字节数，0表示EOF，失败时返回-1。注意，错误码可能指示非阻塞操作不能立即成功，应该检查错误码`EAGAIN`（或者Windows中的`WSAWOULDBLOCK`）。如果`howmuch`为负，`evbuffer_read()` 会尝试猜测要读取多少数据。
`evbuffer_write_atmost()`函数试图将`buffer`前面至多`howmuch`字节写入到套接字`fd`中。成功时函数返回写入的字节数，失败时返回-1。跟`evbuffer_read()`一样，应该检查错误码，看是真的错误，还是仅仅指示非阻塞IO不能立即完成。如果为`howmuch`给出负值，函数会试图写入`buffer`的所有内容。
调用`evbuffer_write()`与使用负的`howmuch`参数调用`evbuffer_write_atmost()`一样：函数会试图尽量清空`buffer`的内容。
在Unix中，这些函数应该可以在任何支持`read`和`write`的文件描述符上正确工作。在Windows中，仅仅支持套接字。
注意，如果使用`bufferevent`，则不需要调用这些函数，`bufferevent`的代码已经为你调用了。
`evbuffer_write_atmost()`函数在2.0.1-alpha版本中引入。



##  为基于evbuffer的IO避免数据复制

真正高速的网络编程通常要求尽量少的数据复制，libevent为此提供了一些机制：

```
typedef void (*evbuffer_ref_cleanup_cb)(const void *data,
    size_t datalen, void *extra);

int evbuffer_add_reference(struct evbuffer *outbuf,
    const void *data, size_t datlen,
    evbuffer_ref_cleanup_cb cleanupfn, void *extra);123456
```

这个函数通过引用向`evbuffer`末尾添加一段数据。不会进行复制：`evbuffer`只会存储一个到`data`处的`datlen`字节的指针。因此，在`evbuffer`使用这个指针期间，必须保持指针是有效的。`evbuffer`会在不再需要这部分数据的时候调用用户提供的`cleanupfn`函数，带有提供的`data`指针、`datlen`值和`extra`指针参数。函数成功时返回0，失败时返回-1。

