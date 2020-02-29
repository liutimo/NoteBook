# bufferevent



学习`bufferevent`前先学习`evbuffer`。前者是在后者的基础上进行更高层次的封装。

## evbuffer

> evbuffer是libevent中的一个数据缓冲区，可以保存待接收/发送的数据(通常是网络数据)。
>
> 它的出现，主要是尽可能避免频繁的内存拷贝。



先看下`evbuffer`的结构体定义：

```c++
struct evbuffer {
	struct evbuffer_chain *first;
	struct evbuffer_chain *last;
	struct evbuffer_chain **last_with_datap;
	size_t total_len;		//存储在buffer中所有数据的长度
	size_t max_read;		//每次读操作最多读取的字节数。默认4096
	size_t n_add_for_cb;
	size_t n_del_for_cb;
	unsigned own_lock : 1;
	unsigned freeze_start : 1;
	unsigned freeze_end : 1;
	unsigned deferred_cbs : 1;
	ev_uint32_t flags;
	struct event_base *cb_queue;
	int refcnt;
	struct event_callback deferred;
	LIST_HEAD(evbuffer_cb_queue, evbuffer_cb_entry) callbacks;
	struct bufferevent *parent;
};

struct evbuffer_chain {
	struct evbuffer_chain *next;
	size_t buffer_len;
	ev_misalign_t misalign;
	size_t off;
	unsigned flags;
	int refcnt;
	unsigned char *buffer;
};
```





evbuffer相关的常用函数如下：

- `evbuffer_new`

- `evbuffer_free`

  这两个函数成对使用，用于创建和销毁`evbuffer`。通常，我们不会直接使用这两个接口，其会在`bufferevent`的相关接口内被调用。

- `evbuffer_add`

  在`evbuffer`的尾部追加数据。如果空间不足，会首先调用`evbuffer_expand`扩展内存空间。

  ```c++
  //Append data to the end of an evbuffer.
  int evbuffer_add(struct evbuffer *buf, const void *data, size_t datlen);
  ```

- `evbuffer_add_buffer`

  

结构如下：

```c++
struct bufferevent {
	struct event_base *ev_base;
	const struct bufferevent_ops *be_ops;
	struct event ev_read;	//读事件
	struct event ev_write;  //写事件

	struct evbuffer *input;	//输入缓冲区，接收到的数据
	struct evbuffer *output;//输出缓冲区，需要发送的数据

	struct event_watermark wm_read;		//读水平线
	struct event_watermark wm_write;	//写水平线

	bufferevent_data_cb readcb;			//读回调函数
	bufferevent_data_cb writecb;		//写回调函数
    
	bufferevent_event_cb errorcb;		//错误回调函数
	void *cbarg;						//回调函数参数

	struct timeval timeout_read;		//读超时
	struct timeval timeout_write;		//写超时
    
	short enabled;
};

struct event_watermark {
	size_t low;			//低水平位
	size_t high;		//高水平位
};
```

接下来通过分析源码来详细了解每一个成员的实际作用。



1. `bufferevent_socket_new`：在给定的socket文件描述符上创建一个bufferevent。

   ```c++
   
   struct bufferevent *
   bufferevent_socket_new(struct event_base *base, evutil_socket_t fd,
       int options)
   {
   	struct bufferevent_private *bufev_p;
   	struct bufferevent *bufev;
   	   
   	if ((bufev_p = mm_calloc(1, sizeof(struct bufferevent_private)))== NULL)
   		return NULL;
   
   	if (bufferevent_init_common_(bufev_p, base, &bufferevent_ops_socket,
   				    options) < 0) {
   		mm_free(bufev_p);
   		return NULL;
   	}
   	bufev = &bufev_p->bev;
   	evbuffer_set_flags(bufev->output, EVBUFFER_FLAG_DRAINS_TO_FD);
   
   	event_assign(&bufev->ev_read, bufev->ev_base, fd,
   	    EV_READ|EV_PERSIST|EV_FINALIZE, bufferevent_readcb, bufev);
   	event_assign(&bufev->ev_write, bufev->ev_base, fd,
   	    EV_WRITE|EV_PERSIST|EV_FINALIZE, bufferevent_writecb, bufev);
   
   	evbuffer_add_cb(bufev->output, bufferevent_socket_outbuf_cb, bufev);
   
   	evbuffer_freeze(bufev->input, 0);
   	evbuffer_freeze(bufev->output, 1);
   
   	return bufev;
   }
   ```

   

   