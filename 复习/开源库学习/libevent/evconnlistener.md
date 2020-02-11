# evconnlistener



## 定义

```c++
struct evconnlistener_ops {
    //加入事件循环
	int (*enable)(struct evconnlistener *);
    //从事件循环中移除
	int (*disable)(struct evconnlistener *);
    //从事件循环中移除，并且关闭socket
	void (*destroy)(struct evconnlistener *);
    //无实现
	void (*shutdown)(struct evconnlistener *);
	evutil_socket_t (*getfd)(struct evconnlistener *);
	struct event_base *(*getbase)(struct evconnlistener *);
};

struct evconnlistener {
	const struct evconnlistener_ops *ops;  //操作函数集
	void *lock;		//锁
	evconnlistener_cb cb; 
	evconnlistener_errorcb errorcb;
	void *user_data;
	unsigned flags;
	short refcnt; //引用计数，每有一个新连接进来，都会+1，调用相应的callback后 -1
	int accept4_flags;
	unsigned enabled : 1;
};

```



## 创建过程

1. `evconnlistener_new`: 在已有的文件描述符上创建

   ```c++
   struct evconnlistener *evconnlistener_new(struct event_base *base,
       evconnlistener_cb cb, void *ptr, unsigned flags, int backlog,
       evutil_socket_t fd);
   ```

   - `evconnlistener_cb cb`

     当有新连接进入时会调用该回调函数。

   - `void *ptr`

     用户数据，会被传入到回调函数中去，通常是`event_base`指针。

   - `unsigned flags`

     ```c++
     #define LEV_OPT_LEAVE_SOCKETS_BLOCKING	(1u<<0)
     #define LEV_OPT_CLOSE_ON_FREE		(1u<<1)
     #define LEV_OPT_CLOSE_ON_EXEC		(1u<<2)
     #define LEV_OPT_REUSEABLE		(1u<<3)
     #define LEV_OPT_THREADSAFE		(1u<<4)
     #define LEV_OPT_DISABLED		(1u<<5)
     #define LEV_OPT_DEFERRED_ACCEPT		(1u<<6)
     #define LEV_OPT_REUSEABLE_PORT		(1u<<7)
     #define LEV_OPT_BIND_IPV6ONLY		(1u<<8)
     ```

   - `int backlog`

     用于确定`listen`的第二个参数。如果为0，表示当前socket处于listening状态。

   - `evutil_socket_t fd`

     需要listen的文件描述符。必须为非阻塞状态，并且已经绑定端口和地址。

   现在来看其实现

   ```c++
   struct evconnlistener *
   evconnlistener_new(struct event_base *base,
       evconnlistener_cb cb, void *ptr, unsigned flags, int backlog,
       evutil_socket_t fd)
   {
   	struct evconnlistener_event *lev;
       //backlog的作用
   	if (backlog > 0) {
   		if (listen(fd, backlog) < 0)
   			return NULL;evconnlistener_new
   	} else if (backlog < 0) {
   		if (listen(fd, 128) < 0)
   			return NULL;
   	}
   
       //分配evconnlistener_event内存
   	lev = mm_calloc(1, sizeof(struct evconnlistener_event));
   	if (!lev)
   		return NULL;
   
       //初始化操作咯
   	lev->base.ops = &evconnlistener_event_ops;
   	lev->base.cb = cb;
   	lev->base.user_data = ptr;
   	lev->base.flags = flags;
   	lev->base.refcnt = 1;
   
   	lev->base.accept4_flags = 0;
   	if (!(flags & LEV_OPT_LEAVE_SOCKETS_BLOCKING))
   		lev->base.accept4_flags |= EVUTIL_SOCK_NONBLOCK;
   	if (flags & LEV_OPT_CLOSE_ON_EXEC)
   		lev->base.accept4_flags |= EVUTIL_SOCK_CLOEXEC;
   
       //LEV_OPT_THREADSAFE指示listener其线程安全，所以需要锁来保证
   	if (flags & LEV_OPT_THREADSAFE) {
   		EVTHREAD_ALLOC_LOCK(lev->base.lock, EVTHREAD_LOCKTYPE_RECURSIVE);
   	}
   
       //熟悉吧？
   	event_assign(&lev->listener, base, fd, EV_READ|EV_PERSIST,
   	    listener_read_cb, lev);
   
   	if (!(flags & LEV_OPT_DISABLED))
   	    evconnlistener_enable(&lev->base);
   
   	return &lev->base;
   }
   ```

   看到这，`listener_read_cb`所做的应该就是accept connection，并将其添加到事件循环中来咯。阅读源码后发现，其在accept连接后，就会调用`evconnlistener_cb`回调函数来处理新连接。所以，如果我们要保持其长连接，就需要再回调函数内部为其创建event，并添加到事件循环中来。

   ```c++
   static void
   listener_read_cb(evutil_socket_t fd, short what, void *p)
   {
   	struct evconnlistener *lev = p;
   	int err;
   	evconnlistener_cb cb;
   	evconnlistener_errorcb errorcb;
   	void *user_data;
   	LOCK(lev);
   	while (1) {
   		struct sockaddr_storage ss;
   		ev_socklen_t socklen = sizeof(ss);
           //[1] 接受一个连接
   		evutil_socket_t new_fd = evutil_accept4_(fd, (struct sockaddr*)&ss, &socklen, lev->accept4_flags);
   		if (new_fd < 0)
   			break;
   		if (socklen == 0) {
   			/* This can happen with some older linux kernels in
   			 * response to nmap. */
   			evutil_closesocket(new_fd);
   			continue;
   		}
   
   		if (lev->cb == NULL) {
   			evutil_closesocket(new_fd);
   			UNLOCK(lev);
   			return;
   		}
   		++lev->refcnt;
   		cb = lev->cb;
   		user_data = lev->user_data;
   		UNLOCK(lev);
           //调用回调函数处理新来的连接
   		cb(lev, new_fd, (struct sockaddr*)&ss, (int)socklen,
   		    user_data);
   		LOCK(lev);
   		/**
   		 * 其初始值为1，连接进入后+1 == 2
   		 * 在回调函数内部，可能会调用evconnlistener_free，岂会将refcnt -1
   		 * 此时，refcnt == 1
   		 */
   		if (lev->refcnt == 1) {
   			int freed = listener_decref_and_unlock(lev);
   			EVUTIL_ASSERT(freed);
   			return;
   		}
   		--lev->refcnt;
   		if (!lev->enabled) {
   			/* the callback could have disabled the listener */
   			UNLOCK(lev);
   			return;
   		}
   	}
   	err = evutil_socket_geterror(fd);
   	if (EVUTIL_ERR_ACCEPT_RETRIABLE(err)) {
   		UNLOCK(lev);
   		return;
   	}
       //调用错误处理回调函数
   	if (lev->errorcb != NULL) {
   		++lev->refcnt;
   		errorcb = lev->errorcb;
   		user_data = lev->user_data;
   		UNLOCK(lev);
   		errorcb(lev, user_data);
   		LOCK(lev);
   		listener_decref_and_unlock(lev);
   	} else {
   		event_sock_warn(fd, "Error from accept() call");
   		UNLOCK(lev);
   	}
   }
   ```

2. 还有一种方式就是根据给定的地址和端口创建

   ```c++
   struct evconnlistener *evconnlistener_new_bind(struct event_base *base,
       evconnlistener_cb cb, void *ptr, unsigned flags, int backlog,
       const struct sockaddr *sa, int socklen);
   ```

   和`evconnlistener_new`类似，区别就是`evconnlistener_new_bind`内部完成了bind操作，并且，在完成bind后，其还是调用`evconnlistener_new`去完成后面的操作。

   ```c++
   struct evconnlistener *
   evconnlistener_new_bind(struct event_base *base, evconnlistener_cb cb,
       void *ptr, unsigned flags, int backlog, const struct sockaddr *sa,
       int socklen)
   {
   	struct evconnlistener *listener;
   	evutil_socket_t fd;
   	int on = 1;
   	int family = sa ? sa->sa_family : AF_UNSPEC;
   	int socktype = SOCK_STREAM | EVUTIL_SOCK_NONBLOCK;
   	int support_keepalive = 1;
   
   	if (backlog == 0)
   		return NULL;
   
   	if (flags & LEV_OPT_CLOSE_ON_EXEC)
   		socktype |= EVUTIL_SOCK_CLOEXEC;
   	//创建socket
   	fd = evutil_socket_(family, socktype, 0);
   	if (fd == -1)
   		return NULL;
   
   	if (support_keepalive) {
   		if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (void*)&on, sizeof(on))<0)
   			goto err;
   	}
   
   	if (flags & LEV_OPT_REUSEABLE) {
   		if (evutil_make_listen_socket_reuseable(fd) < 0)
   			goto err;
   	}
   
   	if (flags & LEV_OPT_REUSEABLE_PORT) {
   		if (evutil_make_listen_socket_reuseable_port(fd) < 0)
   			goto err;
   	}
   
   	if (flags & LEV_OPT_DEFERRED_ACCEPT) {
   		if (evutil_make_tcp_listen_socket_deferred(fd) < 0)
   			goto err;
   	}
   
   	if (flags & LEV_OPT_BIND_IPV6ONLY) {
   		if (evutil_make_listen_socket_ipv6only(fd) < 0)
   			goto err;
   	}
   
   	if (sa) {
           //bind
   		if (bind(fd, sa, socklen)<0)
   			goto err;
   	}
   	listener = evconnlistener_new(base, cb, ptr, flags, backlog, fd);
   	if (!listener)
   		goto err;
   
   	return listener;
   err:
   	evutil_closesocket(fd);
   	return NULL;
   }
   ```

3. 释放evconnlistener

   ```c++
   void
   evconnlistener_free(struct evconnlistener *lev)
   {
   	LOCK(lev);
   	//置空，后面不再接收连接
       lev->cb = NULL;
   	lev->errorcb = NULL;
   	if (lev->ops->shutdown)
   		lev->ops->shutdown(lev);
       //根据引用计数refcnt来判断是否需要立即回收evconnlistener
   	listener_decref_and_unlock(lev);
   }
   ```

4. 激活evconnlistener。调用`evconnlistener->ops->enable`将其加入事件循环。

   ```c++
   int evconnlistener_enable(struct evconnlistener *lev)
   {
   	int r;
   	LOCK(lev);
   	lev->enabled = 1;
   	if (lev->cb)
   		r = lev->ops->enable(lev);
   	else
   		r = 0;
   	UNLOCK(lev);
   	return r;
   }
   ```

5. disable evconnlistener。调用`evconnlistener->ops->disable将其从事件循环移除。

   ```c++
   int evconnlistener_disable(struct evconnlistener *lev)
   {
   	int r;
   	LOCK(lev);
   	lev->enabled = 0;
   	r = lev->ops->disable(lev);
   	UNLOCK(lev);
   	return r;
   }
   ```

