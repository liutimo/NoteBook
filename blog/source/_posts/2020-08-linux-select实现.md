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

   

### 实现

1. 系统调用

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

2. `core_sys_select`

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

3. `do_select`

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

   



