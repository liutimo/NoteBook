---
title: linux-IO复用机制(select/poll/epoll)实现
permalink: linux-select实现
date: 2020-08-20 20:28:43
tags: 	- linux驱动
		- IO复用机制实现
categories: linux 内核		
---

 



## 驱动程序如何支持select/poll/epoll

驱动程序通过实现`file_operations::poll`方法来支持linux的IO复用机制。其原型如下：

```c++
// linux/fs.h
unsigned int (*poll) (struct file *, struct poll_table_struct *);
```

如何实现呢？

首先在设备的`dev`结构中添加两个(根据实际情况决定)等待队列。

```c++
/*mem设备描述结构体*/
struct my_dev                                     
{                                                        
  	char *data;                      
  	unsigned long size; 
  	wait_queue_head_t in_wq;		//可读等待队列	  
	wait_queue_head_t out_wq;		//可写等待队列
};

static int mydev_init(void) {
    ...
    my_dev *mydev = kmalloc(sizeof(struct my_dev), GFP_KERNEL);
    ...
     /*初始化等待队列*/
     init_waitqueue_head(&mydev->in_wq);
     init_waitqueue_head(&mydev->out_wq);
    ...
}
```

完成初始化操作后，就可以实现poll方法了。

```c++
unsigned int my_poll (struct file *filp, struct poll_table_struct *wait) {
    my_dev *mydev = filp->private_data;
    unsigned int mask = 0;
    
    //重点
    poll_wait(filp, &mydev->in_wq, wait);
    poll_wait(filp, &mydev->in_wq, wait);
    
    if (in) {
        mask |= POLL_IN | POLLRDNORM;
    }
    if (out) {
        mask |= POLL_OUT;
    }
    return mask;
}
```

`poll`方法需要返回相应掩码来指示驱动数据是否可读写。

其实现调用的`poll_wait`函数是`poll`的核心。linux的IO复用机制就是通过调用相应设备的驱动实现的。



## 实现select/poll的一些辅助结构

### 重要的数据结构

1. `poll_table`

   ```c++
   /*
    * Do not touch the structure directly, use the access functions
    * poll_does_not_wait() and poll_requested_events() instead.
    */
   typedef struct poll_table_struct {
   	poll_queue_proc _qproc;
   	unsigned long _key;
   } poll_table;
   ```

2. `poll_wqueues`

   ```c++
   /* ~832 bytes of stack space used max in sys_select/sys_poll before allocating
      additional memory. */
   #define MAX_STACK_ALLOC 832
   #define FRONTEND_STACK_ALLOC	256
   #define SELECT_STACK_ALLOC	FRONTEND_STACK_ALLOC
   #define POLL_STACK_ALLOC	FRONTEND_STACK_ALLOC
   #define WQUEUES_STACK_ALLOC	(MAX_STACK_ALLOC - FRONTEND_STACK_ALLOC)
   #define N_INLINE_POLL_ENTRIES	(WQUEUES_STACK_ALLOC / sizeof(struct poll_table_entry))
   /*
    * Structures and helpers for select/poll syscall
    */
   //每一个执行select/poll的线程都会创建一个该结构。
   struct poll_wqueues {
   	poll_table pt;		//额，这个用于保存一个唤醒函数的地址
   	struct poll_table_page *table;  //当inline_entries不够用时，在申请堆内存
   	struct task_struct *polling_task;	//指向创建线程，后面用于初始化wait_queue_t
   	int triggered;	//用于标志 有事件发生
   	int error;
   	int inline_index;  //记录当前空闲的的 poll_table_entry下班， 当 inline_index > N_INLINE_POLL_ENTRIES时，就要申请内存了
   	struct poll_table_entry inline_entries[N_INLINE_POLL_ENTRIES];   //预先在stack space分配几个 poll_table_entry
   };
   ```

3. `poll_table_entry`

   ```c++
   struct poll_table_entry {
   	struct file *filp;
   	unsigned long key;
   	wait_queue_t wait;	
   	wait_queue_head_t *wait_address;  //当前entry 被挂入的等待队列
   };
   ```

### 重要函数

从`poll_wait`函数入手。

```c++
static inline void poll_wait(struct file * filp, wait_queue_head_t * wait_address, poll_table *p)
{
	//p就是fop->poll的第二个参数。
	if (p && p->_qproc && wait_address)
		p->_qproc(filp, wait_address, p);
}
```

这里我们走读一哈源码很容易晓得 `_qproc`指向的函数就是`__pollwait`。

```c++
static void __pollwait(struct file *filp, wait_queue_head_t *wait_address,
				poll_table *p)
{
    //通过指针 poll_table 拿到 相应的 poll_wqueues
	struct poll_wqueues *pwq = container_of(p, struct poll_wqueues, pt);
    //新建或者从缓存中获取一个 poll_table_entry。 
	struct poll_table_entry *entry = poll_get_entry(pwq);
	if (!entry)
		return; //出错直接返回
	entry->filp = get_file(filp);			//增加引用计数
	entry->wait_address = wait_address;		
	entry->key = p->_key;
    
    //初始化 entry  的 wait_queue_t
    //等待队列的wake_up函数就是通过调用每个wait_queue_t->func来唤醒进程的。
	init_waitqueue_func_entry(&entry->wait, pollwake);
	entry->wait.private = pwq;
    //将其添加到驱动的等待队列。
	add_wait_queue(wait_address, &entry->wait);
}
```

从上面的代码我们可以看到，`poll_wait`的并不会休眠进程，其只是将与`poll_table`关联的一个`wait_queue_t`添加到驱动程序的等待队列中。当驱动数据可用时，就会通过`wake_up`来唤醒等待队列中的等待进程。

> 和我们平常使用等待队列有点不一样，我们之前添加进程到等待对列是通过 `wait_event_interruptible`这种函数进行的，该函数阻塞进程知道条件成立。这里，`poll-wait`缺失直接将进程添加到等待队列中，并未进行休眠操作。

这些函数的实现本来就是为了更好的实现IO复用机制，所以，上面的问题在`select`和`poll`的实现中肯定能找到答案。先来看看 当调用`pollwake`是如何唤醒与`poll_table`关联的进程的。

```c++
static int __pollwake(wait_queue_t *wait, unsigned mode, int sync, void *key)
{
	struct poll_wqueues *pwq = wait->private;
	DECLARE_WAITQUEUE(dummy_wait, pwq->polling_task);

	/*
	 * Although this function is called under waitqueue lock, LOCK
	 * doesn't imply write barrier and the users expect write
	 * barrier semantics on wakeup functions.  The following
	 * smp_wmb() is equivalent to smp_wmb() in try_to_wake_up()
	 * and is paired with smp_store_mb() in poll_schedule_timeout.
	 */
	smp_wmb();
	pwq->triggered = 1;

	/*
	 * Perform the default wake up operation using a dummy
	 * waitqueue.
	 *
	 * TODO: This is hacky but there currently is no interface to
	 * pass in @sync.  @sync is scheduled to be removed and once
	 * that happens, wake_up_process() can be used directly.
	 */
	return default_wake_function(&dummy_wait, mode, sync, key);
}
```



`default_wake_function`最终就会调用`try_to_wake_up`去唤醒`wait_queue_t->private`所指定的进程。

`__pollwait`执行时，将会将一个从`poll_wqueues`获取一个`wait_queue_t`添加到其驱动的等待队列中，这个`wait_queue_t`的`private`字段被初始化为`null`，而其唤醒函数是`pollwake`，`pollwake`又创建了一个`wait_queue_t`，并将其`private`字段初始化为`poll_wqueues->polling_task`，但是这里，并没有将其添加到等待队列中，而是直接调用`default_wake_function`去唤醒`poll_wqueues->polling_task`所指示的进程。

有了这些知识，再去看`select`和`poll`的实现就要容易很多了。







## select的实现



### 问题

1. select 最多支持多少个文件描述符？

   首先，从`core_sys_select`中可以知道，`select`的第一个参数只是做一个用户层的约束（通常是我们的最大`fd` + 1），实际上，我们可以+2,+3, +100...
   
   最终驱动都会将其赋值为`file->fdt->max_fds`   
   
   然后：
   
   ```c++
   // sys/select.h
   /* Maximum number of file descriptors in `fd_set'.  */
   #define	FD_SETSIZE		__FD_SETSIZE
   /* Number of descriptors that can fit in an `fd_set'.  */
   #define __FD_SETSIZE		1024
   ```
   
   这里说的最多1024，实际上呢？其只是指示`fd_set `只能容纳最大为`1023`的文件描述符。但是，驱动内部并没有对这个`FD_SETSIZE`的做校验，所以我们可以通过动态内存来使得`select`支持更多的文件描述符。

### 实现

#### 系统调用

```c++
SYSCALL_DEFINE5(select, int, n, fd_set __user *, inp, fd_set __user *, outp,
		fd_set __user *, exp, struct timeval __user *, tvp)
{
	struct timespec end_time, *to = NULL;
	struct timeval tv;
	int ret;

	//将超时时间计算一个具体时间点(以系统启动时间为准)。
	if (tvp) {
		if (copy_from_user(&tv, tvp, sizeof(tv)))
			return -EFAULT;

		to = &end_time;
		//[1] 将timeval 转换成 timespec      t1
		//[2] 获取当前CLOCK_MONOTONIC 时间的 timespec	t2
		//[3] to = t1 + t2
		if (poll_select_set_timeout(to,
				tv.tv_sec + (tv.tv_usec / USEC_PER_SEC),
				(tv.tv_usec % USEC_PER_SEC) * NSEC_PER_USEC))
			return -EINVAL;
	}

	//完成select的核心操作
	ret = core_sys_select(n, inp, outp, exp, to);
	//计算剩余时间，并将拷贝到用户空间中的 tvp
	ret = poll_select_copy_remaining(&end_time, tvp, 1, ret);
	return ret;
}
```

#### `core_sys_select`

```c++
int core_sys_select(int n, fd_set __user *inp, fd_set __user *outp,
			   fd_set __user *exp, struct timespec *end_time)
{
	fd_set_bits fds;
	void *bits;
	int ret, max_fds;
	size_t size, alloc_size;  
	struct fdtable *fdt;
	/* Allocate small arguments on the stack to save memory and be faster */
	long stack_fds[SELECT_STACK_ALLOC/sizeof(long)];

	ret = -EINVAL;
	if (n < 0)
		goto out_nofds;

	/* max_fds can increase, so grab it once to avoid race */
	rcu_read_lock();
	/**
		struct task_struct {
			...
			// open file information 
			struct files_struct *files;
			...
		};
	*/
	fdt = files_fdtable(current->files);
	max_fds = fdt->max_fds;
	rcu_read_unlock();
	if (n > max_fds)
		n = max_fds;  //大于max_fds就将其设置为max_fds

	/*
	 * We need 6 bitmaps (in/out/ex for both incoming and outgoing),
	 * since we used fdset we need to allocate memory in units of
	 * long-words. 
	 */
	 
	//计算需要多少个字节才能保存1 ~ n的文件描述符。 
	//实测 n == 64  size == 8   n == 65 size == 16
	size = FDS_BYTES(n);	
	bits = stack_fds;
    //这里，可以看出，select最多支持 (unsigned long) (~(size_t)0) / 6 个文件描述符
	if (size > sizeof(stack_fds) / 6) {
		/* Not enough space in on-stack array; must use kmalloc */
		ret = -ENOMEM;
		if (size > (SIZE_MAX / 6))
			goto out_nofds;
		//创建以一块缓冲区
		alloc_size = 6 * size;
		bits = kmalloc(alloc_size, GFP_KERNEL|__GFP_NOWARN);
		if (!bits && alloc_size > PAGE_SIZE)
			bits = vmalloc(alloc_size);

		if (!bits)
			goto out_nofds;
	}
	fds.in      = bits;
	fds.out     = bits +   size;
	fds.ex      = bits + 2*size;
	fds.res_in  = bits + 3*size;
	fds.res_out = bits + 4*size;
	fds.res_ex  = bits + 5*size;

	//将用户层参数拷贝到内核层
	if ((ret = get_fd_set(n, inp, fds.in)) ||
	    (ret = get_fd_set(n, outp, fds.out)) ||
	    (ret = get_fd_set(n, exp, fds.ex)))
		goto out;
	//全部置零
	zero_fd_set(n, fds.res_in);
	zero_fd_set(n, fds.res_out);
	zero_fd_set(n, fds.res_ex);

	//核心工作
	ret = do_select(n, &fds, end_time);

	if (ret < 0)
		goto out;
	if (!ret) {
		ret = -ERESTARTNOHAND;
		if (signal_pending(current))
			goto out;
		ret = 0;
	}

	//拷贝结果到用户空间。 (1)
	if (set_fd_set(n, inp, fds.res_in) ||
	    set_fd_set(n, outp, fds.res_out) ||
	    set_fd_set(n, exp, fds.res_ex))
		ret = -EFAULT;

out:
	if (bits != stack_fds)
		kvfree(bits);
out_nofds:
	return ret;
}

```

#### `do_select`
```c++
int do_select(int n, fd_set_bits *fds, struct timespec *end_time)
{
	ktime_t expire, *to = NULL;
	struct poll_wqueues table;
	poll_table *wait;
	int retval, i, timed_out = 0;
	u64 slack = 0;
	unsigned int busy_flag = net_busy_loop_on() ? POLL_BUSY_LOOP : 0;
	unsigned long busy_end = 0;

	rcu_read_lock();
	//从fds中，找到最大文件描述符
	retval = max_select_fd(n, fds);
	rcu_read_unlock();

	if (retval < 0)
		return retval;
	n = retval;

	poll_initwait(&table);
	wait = &table.pt;
	if (end_time && !end_time->tv_sec && !end_time->tv_nsec) {
		wait->_qproc = NULL;
		timed_out = 1;
	}

	if (end_time && !timed_out)
		slack = select_estimate_accuracy(end_time);

	retval = 0;
	for (;;) {
		unsigned long *rinp, *routp, *rexp, *inp, *outp, *exp;
		bool can_busy_loop = false;
		inp = fds->in; outp = fds->out; exp = fds->ex;
		rinp = fds->res_in; routp = fds->res_out; rexp = fds->res_ex;
		for (i = 0; i < n; ++rinp, ++routp, ++rexp) {
			unsigned long in, out, ex, all_bits, bit = 1, mask, j;
			unsigned long res_in = 0, res_out = 0, res_ex = 0;
			in = *inp++; out = *outp++; ex = *exp++;
			//当前循环没有一个文件描述符被添加到 select中
			all_bits = in | out | ex;
			if (all_bits == 0) {
				i += BITS_PER_LONG;
				continue;
			}
			//在32bit中，找到添加到select中的fd
			for (j = 0; j < BITS_PER_LONG; ++j, ++i, bit <<= 1) {
				struct fd f;
				if (i >= n)
					break;
				if (!(bit & all_bits))
					continue;
				//拿到我们关心的fd的对应的 file_operations 指针。
				f = fdget(i);
				if (f.file) {
					const struct file_operations *f_op;
					f_op = f.file->f_op;
					mask = DEFAULT_POLLMASK;
					//poll函数不为空
					if (f_op->poll) {
						wait_key_set(wait, in, out,
							     bit, busy_flag);
						//这里，调用驱动程序的poll方法来处理当前文件描述符。
						//进而调用poll_wait. 
						// poll_wait实际上调用的就是 wait->_qproc 也就是
						// 我们前面初始化poll_wqueues指定的 __pollwait函数，
						//该函数会从 poll_wqueues中获取一个 poll_table_entry。
						// 然后将 poll_table_entry->wait 挂载到 驱动程序的等待队列中。
						// 其唤醒函数为 pollwake
						//pollwake 的实现就是创建一个wait_queue_t，并初始化其private为 poll_wqueues->polling_task 也就是执行
						//select 的进程。然后调用 default_wake_function 直接唤醒。
						mask = (*f_op->poll)(f.file, wait);
					}
					fdput(f);
					//判断是否有激活的事件
					if ((mask & POLLIN_SET) && (in & bit)) {
						res_in |= bit;
						retval++;
						wait->_qproc = NULL;
					}
					if ((mask & POLLOUT_SET) && (out & bit)) {
						res_out |= bit;
						retval++;
						wait->_qproc = NULL;
					}
					if ((mask & POLLEX_SET) && (ex & bit)) {
						res_ex |= bit;
						retval++;
						wait->_qproc = NULL;
					}
					if (retval) {
						can_busy_loop = false;
						busy_flag = 0;
					} else if (busy_flag & mask)
						can_busy_loop = true;

				}
			}
			if (res_in)
				*rinp = res_in;
			if (res_out)
				*routp = res_out;
			if (res_ex)
				*rexp = res_ex;
			cond_resched();
		}
		wait->_qproc = NULL;
		if (retval || timed_out || signal_pending(current))
			break;
		if (table.error) {
			retval = table.error;
			break;
		}
		if (can_busy_loop && !need_resched()) {
			if (!busy_end) {
				busy_end = busy_loop_end_time();
				continue;
			}
			if (!busy_loop_timeout(busy_end))
				continue;
		}
		busy_flag = 0;
		if (end_time && !to) {
			expire = timespec_to_ktime(*end_time);
			to = &expire;
		}
		//哈哈，这里进入休眠， 如何唤醒呢？
		//驱动程序调用 wake_up_event, 进而调用 pollwake  然后就唤醒了
		if (!poll_schedule_timeout(&table, TASK_INTERRUPTIBLE,
					   to, slack))
			timed_out = 1;
	}

	poll_freewait(&table);

	return retval;
}
```





## poll的实现

看了`select`的实现，可以想象，其内部原理应该是一样的。通过调用驱动的`poll`函数来判断是否有事件触发。

大致步骤如下：

为每个fd创建一个`poll_table_entry`，并通过驱动的`poll_wait`方法将其添加到驱动的等待对列中（唤醒函数为`pollwake`）。

当驱动数据可读时，会调用`pollwake`来唤醒调用`poll`的进程，该函数会将调用`default_wake_function`唤醒`poll_table_entry->filp->polling_task`指示的进程。



`poll`相比`select`的改进点在于其只遍历时只需遍历应用程序关心的文件描述符，而不需要像`select`一样去判断无关的文件描述符。

```c++
for (i = 0; i < n; ++rinp, ++routp, ++rexp) {
    //在32bit中，找到添加到select中的fd
    for (j = 0; j < BITS_PER_LONG; ++j, ++i, bit <<= 1) {
        struct fd f;
        if (i >= n)
            break;
        //select 这里需要额外判断一哈， i是否使我们关心的文件描述符。
        if (!(bit & all_bits))
            continue;
        ...
    }
}
```



### 实现

#### 系统调用

```c++

SYSCALL_DEFINE3(poll, struct pollfd __user *, ufds, unsigned int, nfds,
		int, timeout_msecs)
{
	struct timespec end_time, *to = NULL;
	int ret;
	
    //将超时时间转换为时间点
	if (timeout_msecs >= 0) {
		to = &end_time;
		poll_select_set_timeout(to, timeout_msecs / MSEC_PER_SEC,
			NSEC_PER_MSEC * (timeout_msecs % MSEC_PER_SEC));
	}
	
    //核心工作
	ret = do_sys_poll(ufds, nfds, to);

    //被信号中断，重新启动系统调用。由libc库完成
	if (ret == -EINTR) {
		struct restart_block *restart_block;

		restart_block = &current->restart_block;
		restart_block->fn = do_restart_poll;
		restart_block->poll.ufds = ufds;
		restart_block->poll.nfds = nfds;

		if (timeout_msecs >= 0) {
			restart_block->poll.tv_sec = end_time.tv_sec;
			restart_block->poll.tv_nsec = end_time.tv_nsec;
			restart_block->poll.has_timeout = 1;
		} else
			restart_block->poll.has_timeout = 0;

		ret = -ERESTART_RESTARTBLOCK;
	}
	return ret;
}

```

#### do_sys_poll

```c++
#define N_STACK_PPS ((sizeof(stack_pps) - sizeof(struct poll_list))  / \
			sizeof(struct pollfd))

int do_sys_poll(struct pollfd __user *ufds, unsigned int nfds,
		struct timespec *end_time)
{
	struct poll_wqueues table;
 	int err = -EFAULT, fdcount, len, size;
	/* Allocate small arguments on the stack to save memory and be
	   faster - use long to make sure the buffer is aligned properly
	   on 64 bit archs to avoid unaligned access */
	long stack_pps[POLL_STACK_ALLOC/sizeof(long)];   //256 bytes
	struct poll_list *const head = (struct poll_list *)stack_pps;
 	struct poll_list *walk = head;
 	unsigned long todo = nfds;

	if (nfds > rlimit(RLIMIT_NOFILE))
		return -EINVAL;

	//N_STACK_PPS 用于计算 stack_pps 能够保存的 pollfd 个数
	len = min_t(unsigned int, nfds, N_STACK_PPS);
	for (;;) {
		walk->next = NULL;
		walk->len = len;
		if (!len)
			break;

		if (copy_from_user(walk->entries, ufds + nfds-todo,
					sizeof(struct pollfd) * walk->len))
			goto out_fds;

		//还有没拷贝完的
		todo -= walk->len;
		if (!todo)
			break;

		//计算一个page能保存几个 pollfd
		len = min(todo, POLLFD_PER_PAGE);
		size = sizeof(struct poll_list) + sizeof(struct pollfd) * len;
		walk = walk->next = kmalloc(size, GFP_KERNEL);
		if (!walk) {
			err = -ENOMEM;
			goto out_fds;
		}
	}

    //核心代码
	poll_initwait(&table);
	fdcount = do_poll(nfds, head, &table, end_time);
	poll_freewait(&table);

    //将触发的pollfd拷贝回用户空间
	for (walk = head; walk; walk = walk->next) {
		struct pollfd *fds = walk->entries;
		int j;

		for (j = 0; j < walk->len; j++, ufds++)
			if (__put_user(fds[j].revents, &ufds->revents))
				goto out_fds;
  	}

	err = fdcount;
out_fds:
	walk = head->next;
	while (walk) {
		struct poll_list *pos = walk;
		walk = walk->next;
		kfree(pos);
	}
	return err;
}
```

这里的`poll_list`结构和`poll_table_entry`的内存模型类似。在栈内存上留一小片保存`pollfd`结构。不够的话在申请一个page的内存。直到能够保存所有的`poll_fd`。

```c++
struct poll_list {
	struct poll_list *next;  //执行下一个poll_list的位置
	int len;	//当前poll_list上容纳的 pollfd的数量
	struct pollfd entries[0]; 
};
```



#### do_poll

```c++
static int do_poll(unsigned int nfds,  struct poll_list *list,
		   struct poll_wqueues *wait, struct timespec *end_time)
{	
	//需要传递给驱动的poll函数作为参数
	poll_table* pt = &wait->pt;
	ktime_t expire, *to = NULL;
	int timed_out = 0, count = 0;
	u64 slack = 0;
	unsigned int busy_flag = net_busy_loop_on() ? POLL_BUSY_LOOP : 0;
	unsigned long busy_end = 0;

	/* Optimise the no-wait case */
	if (end_time && !end_time->tv_sec && !end_time->tv_nsec) {
		pt->_qproc = NULL;
		timed_out = 1;
	}

	if (end_time && !timed_out)
		slack = select_estimate_accuracy(end_time);

	for (;;) {
		struct poll_list *walk;
		bool can_busy_loop = false;
		//这里就是较select有所改善的地方。
		for (walk = list; walk != NULL; walk = walk->next) {
			struct pollfd * pfd, * pfd_end;

			pfd = walk->entries;
			pfd_end = pfd + walk->len;
			for (; pfd != pfd_end; pfd++) {
				/*
				 * Fish for events. If we found one, record it
				 * and kill poll_table->_qproc, so we don't
				 * needlessly register any other waiters after
				 * this. They'll get immediately deregistered
				 * when we break out and return.
				 */
				if (do_pollfd(pfd, pt, &can_busy_loop,
					      busy_flag)) {
					count++;
					pt->_qproc = NULL;
					/* found something, stop busy polling */
					busy_flag = 0;
					can_busy_loop = false;
				}
			}
		}
		/*
		 * All waiters have already been registered, so don't provide
		 * a poll_table->_qproc to them on the next loop iteration.
		 */
		pt->_qproc = NULL;
		if (!count) {
			count = wait->error;
			if (signal_pending(current))
				count = -EINTR;
		}
		if (count || timed_out)
			break;

		/* only if found POLL_BUSY_LOOP sockets && not out of time */
		if (can_busy_loop && !need_resched()) {
			if (!busy_end) {
				busy_end = busy_loop_end_time();
				continue;
			}
			if (!busy_loop_timeout(busy_end))
				continue;
		}
		busy_flag = 0;

		/*
		 * If this is the first loop and we have a timeout
		 * given, then we convert to ktime_t and set the to
		 * pointer to the expiry value.
		 */
		if (end_time && !to) {
			expire = timespec_to_ktime(*end_time);
			to = &expire;
		}

		if (!poll_schedule_timeout(wait, TASK_INTERRUPTIBLE, to, slack))
			timed_out = 1;
	}
	return count;
}
```



#### do_pollfd

```c++
/*
 * Fish for pollable events on the pollfd->fd file descriptor. We're only
 * interested in events matching the pollfd->events mask, and the result
 * matching that mask is both recorded in pollfd->revents and returned. The
 * pwait poll_table will be used by the fd-provided poll handler for waiting,
 * if pwait->_qproc is non-NULL.
 */
static inline unsigned int do_pollfd(struct pollfd *pollfd, poll_table *pwait,
				     bool *can_busy_poll,
				     unsigned int busy_flag)
{
	unsigned int mask;
	int fd;

	mask = 0;
	fd = pollfd->fd;
	if (fd >= 0) {
		struct fd f = fdget(fd);
		mask = POLLNVAL;
		if (f.file) {
			mask = DEFAULT_POLLMASK;
			if (f.file->f_op->poll) {
				pwait->_key = pollfd->events|POLLERR|POLLHUP;
				pwait->_key |= busy_flag;
				mask = f.file->f_op->poll(f.file, pwait);
				if (mask & busy_flag)
					*can_busy_poll = true;
			}
			/* Mask out unneeded events. */
			mask &= pollfd->events | POLLERR | POLLHUP;
			fdput(f);
		}
	}
	pollfd->revents = mask;

	return mask;
}
```

这里和`select`基本一致了。



## select和poll的区别

1. `select`使用位掩码，而`poll`使用的时`pollfd`。

   前者通过传入3个`fd_set`来分别获取读、写、错误事件，后者通过`pollfd`的`events`来设置其关心的事件，`revents`来获取其触发的事件。

   当`fd<1024`时，`select`每次传入内核的数据的`size` 为 `sizeof(timeval) + 3 * sizeof(fd_set)`

   `poll`传入内核的`size`为`n * sizeof(pollfd)`

   在遍历时，`select`和`poll`的时间复杂度都为O(n)。

   如何选择`select`和`poll`?

   从上面的分析，`select`内存方面似乎占优势。

   `sizeof(fd_set) == 128 sizeof(pollfd) == 8`

   假设存在1024个fd。

   `select`的需要拷贝的内存为`3 * sizeof(fd_set) + sizeof(timeval)` 

   `poll`需要拷贝的内存为`1024 * sizeof(pollfd)`。

   显然，`poll`涉及到的内存拷贝更多。

   但是，当fd数量不多时， `select`需要拷贝的内存为`FDS_BYTES(maxFd)`。`poll`为`maxFd * sizeof(pollfd)`

   ```c++
   #define FDS_BITPERLONG	(8*sizeof(long))       //32 or64 
   #define FDS_LONGS(nr)	(((nr)+FDS_BITPERLONG-1)/FDS_BITPERLONG)   // 32 or 64bit对齐
   #define FDS_BYTES(nr)	(FDS_LONGS(nr)*sizeof(long))
   ```

   当存在大量连续的fd的时，显然用`select`更合适，但是当存在数量较少又不连续时,用`poll`更合适。



### epoll的实现

