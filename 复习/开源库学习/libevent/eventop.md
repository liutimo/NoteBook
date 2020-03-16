## eventop

哈哈，一开始看成了event top。还在想，什么鬼呢，仔细一下看，event operator。。。。



libevent支持跨平台的，为了在各个OS都能达到很高的效率，就需要选择最适合的IO复用技术。

使用eventop结构将它们统一起来，向外提供功能一致的接口。





### 定义

```C++
/** Structure to define the backend of a given event_base. */
struct eventop {
   /** The name of this backend. */
   const char *name;
   /** Function to set up an event_base to use this backend.  It should
    * create a new structure holding whatever information is needed to
    * run the backend, and return it.  The returned pointer will get
    * stored by event_init into the event_base.evbase field.  On failure,
    * this function should return NULL. */
   void *(*init)(struct event_base *);
   /** Enable reading/writing on a given fd or signal.  'events' will be
    * the events that we're trying to enable: one or more of EV_READ,
    * EV_WRITE, EV_SIGNAL, and EV_ET.  'old' will be those events that
    * were enabled on this fd previously.  'fdinfo' will be a structure
    * associated with the fd by the evmap; its size is defined by the
    * fdinfo field below.  It will be set to 0 the first time the fd is
    * added.  The function should return 0 on success and -1 on error.
    */
   int (*add)(struct event_base *, evutil_socket_t fd, short old, short events, void *fdinfo);
   /** As "add", except 'events' contains the events we mean to disable. */
   int (*del)(struct event_base *, evutil_socket_t fd, short old, short events, void *fdinfo);
   /** Function to implement the core of an event loop.  It must see which
       added events are ready, and cause event_active to be called for each
       active event (usually via event_io_active or such).  It should
       return 0 on success and -1 on error.
    */
   int (*dispatch)(struct event_base *, struct timeval *);
   /** Function to clean up and free our data from the event_base. */
   void (*dealloc)(struct event_base *);
   /** Flag: set if we need to reinitialize the event base after we fork.
    */
   int need_reinit;
   /** Bit-array of supported event_method_features that this backend can
    * provide. */
   enum event_method_feature features;
   /** Length of the extra information we should record for each fd that
       has one or more active events.  This information is recorded
       as part of the evmap entry for each fd, and passed as an argument
       to the add and del functions above.
    */
   size_t fdinfo_len;
};
```



每个函数指针的意义我们结合源码来分析。



####  select

```c++
const struct eventop selectops = {
	"select",		//name
	select_init,
	select_add,
	select_del,
	select_dispatch,
	select_dealloc,
	0, /* doesn't need reinit. */
	EV_FEATURE_FDS,
	0,
};
```

1. `select_init`

   ```c++
   struct selectop {
   	int event_fds;		/* 表示当前最大的fds */
   	int event_fdsz;   /*表示的是fd_set 能容纳的最大fd  该值大于等于fds*/
   	int resize_out_sets; /*初始化为1, 用于标志是否需要realloc 两个 out fd_set*/
   	fd_set *event_readset_in;  /*read*/ //select_del select_add 操作这两个in
   	fd_set *event_writeset_in; /*write*/
   	fd_set *event_readset_out;	//调用select时，传入这两个out
   	fd_set *event_writeset_out;
   };
   
   static void * select_init(struct event_base *base)
   {
   	struct selectop *sop;
   	if (!(sop = mm_calloc(1, sizeof(struct selectop))))
   		return (NULL);
   	//[1] 初始化结构体 selectop
   	if (select_resize(sop, SELECT_ALLOC_SIZE(32 + 1))) {
   		select_free_selectop(sop);
   		return (NULL);
   	}
   	evsig_init_(base);
   	evutil_weakrand_seed_(&base->weakrand_seed, 0);
   	return (sop);
   }
   ```

   `select_resize`:

   其功能时，当需要新增的fd大于当前fd_set的容量时，扩展fd_set。

2. `select_add`

   ```c++
   static int select_add(struct event_base *base, int fd, short old, short events, void *p)
   {
   	struct selectop *sop = base->evbase;
   	EVUTIL_ASSERT((events & EV_SIGNAL) == 0);
   	check_selectop(sop);
     //fd大于当前的最大fd，就需要扩展selectop的内存。
   	if (sop->event_fds < fd) {
   		int fdsz = sop->event_fdsz;
   
   		if (fdsz < (int)sizeof(fd_mask))
   			fdsz = (int)sizeof(fd_mask);
   
   		/* In theory we should worry about overflow here.  In
   		 * reality, though, the highest fd on a unixy system will
   		 * not overflow here. XXXX */
   		while (fdsz < (int) SELECT_ALLOC_SIZE(fd + 1))
   			fdsz *= 2;
   
   		if (fdsz != sop->event_fdsz) {
   			if (select_resize(sop, fdsz)) {
   				check_selectop(sop);
   				return (-1);
   			}
   		}
   
   		sop->event_fds = fd;
   	}
   
   	if (events & EV_READ)
   		FD_SET(fd, sop->event_readset_in);
   	if (events & EV_WRITE)
   		FD_SET(fd, sop->event_writeset_in);
   	check_selectop(sop);
   
   	return (0);
   }
   ```

3. `select_del`

   ```c++
   static int
   select_del(struct event_base *base, int fd, short old, short events, void *p)
   {
   	struct selectop *sop = base->evbase;
   	(void)p;
   
   	EVUTIL_ASSERT((events & EV_SIGNAL) == 0);
   	check_selectop(sop);
   
   	if (sop->event_fds < fd) {
   		check_selectop(sop);
   		return (0);
   	}
   
   	if (events & EV_READ)
   		FD_CLR(fd, sop->event_readset_in);
   
   	if (events & EV_WRITE)
   		FD_CLR(fd, sop->event_writeset_in);
   
   	check_selectop(sop);
   	return (0);
   }
   ```

   很简单的实现，没啥好说的。
   
4. `select_dispatch`

   ```c++
   static int
   select_dispatch(struct event_base *base, struct timeval *tv)
   {
   	int res=0, i, j, nfds;
   	struct selectop *sop = base->evbase;
   	if (sop->resize_out_sets) {
   		fd_set *readset_out=NULL, *writeset_out=NULL;
   		size_t sz = sop->event_fdsz;
   		if (!(readset_out = mm_realloc(sop->event_readset_out, sz)))
   			return (-1);
   		sop->event_readset_out = readset_out;
   		if (!(writeset_out = mm_realloc(sop->event_writeset_out, sz))) {
   			return (-1);
   		}
   		sop->event_writeset_out = writeset_out;
   		sop->resize_out_sets = 0;
   	}
   
   	memcpy(sop->event_readset_out, sop->event_readset_in,
   	       sop->event_fdsz);
   	memcpy(sop->event_writeset_out, sop->event_writeset_in,
   	       sop->event_fdsz);
   
   	nfds = sop->event_fds+1;
   
   	EVBASE_RELEASE_LOCK(base, th_base_lock);
   
   	res = select(nfds, sop->event_readset_out,
   	    sop->event_writeset_out, NULL, tv);
   
   	EVBASE_ACQUIRE_LOCK(base, th_base_lock);
   
     //这里生成一个随机数，从随机数开始调用FD_ISSET。目的是啥呢，不清楚。
   	i = evutil_weakrand_range_(&base->weakrand_seed, nfds);
   	for (j = 0; j < nfds; ++j) {
   		if (++i >= nfds)
   			i = 0;
   		res = 0;
   		if (FD_ISSET(i, sop->event_readset_out))
   			res |= EV_READ;
   		if (FD_ISSET(i, sop->event_writeset_out))
   			res |= EV_WRITE;
   
   		if (res == 0)
   			continue;
   
   		evmap_io_active_(base, i, res);
   	}
   	check_selectop(sop);
   
   	return (0);
   }
   ```

   这里可以看到`event_readset_out`和`event_writeset_out`的作用了。由于`select`每次返回后，都会修改传入的参数。

   

#### epoll

和`select`类似，epoll相关的api也被libevent封装成eventop的形式。其定义为：

```c++
const struct eventop epollops = {
	"epoll",
	epoll_init,
	epoll_nochangelist_add,
	epoll_nochangelist_del,
	epoll_dispatch,
	epoll_dealloc,
	1, /* need reinit */
	EV_FEATURE_ET|EV_FEATURE_O1|EV_FEATURE_EARLY_CLOSE,
	0
};
```

1. `epoll_init`

   