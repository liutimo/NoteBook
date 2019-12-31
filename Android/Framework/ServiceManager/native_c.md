# service_manager进程

`frameworks/native/cmds/servicemanager/service_manager.c`

```c++

int main()
{
    struct binder_state *bs;
	
    //打开binder驱动，并map  128K的kernel memory
    bs = binder_open(128*1024);
    if (!bs) {
        ALOGE("failed to open binder driver\n");
        return -1;
    }

    //成为整个系统中唯一的上下文管理器，其实也就是service管理器
    if (binder_become_context_manager(bs)) {	//[1]
        ALOGE("cannot become context manager (%s)\n", strerror(errno));
        return -1;
    }

    //去除selinux相关代码

    //进入循环
    binder_loop(bs, svcmgr_handler);		//[2]

    return 0;
}

```



main函数的主要工作由`binder_loop`完成。

`[1]`将进程设置为整个系统中唯一的**context manager**。为什么这么做，等分析到`IServiceManager`时应该就会清晰了。

`[2]`，进程的主循环了，其内部通过`ioctl(BINDER_WRITE_READ)`不断地来获取待处理数据，并将数据写回binder驱动(有必要的话)。



`frameworks/native/cmds/servicemanager/binder.c`

```c++
void binder_loop(struct binder_state *bs, binder_handler func)
{
    int res;
    struct binder_write_read bwr;
    uint32_t readbuf[32];   //128字节

    bwr.write_size = 0;
    bwr.write_consumed = 0;
    bwr.write_buffer = 0;

	//[1]
    readbuf[0] = BC_ENTER_LOOPER;
    binder_write(bs, readbuf, sizeof(uint32_t));

    for (;;) {
        bwr.read_size = sizeof(readbuf);
        bwr.read_consumed = 0;
        bwr.read_buffer = (uintptr_t) readbuf;

        //[2]
        res = ioctl(bs->fd, BINDER_WRITE_READ, &bwr);

        if (res < 0) {
            ALOGE("binder_loop: ioctl failed (%s)\n", strerror(errno));
            break;
        }

        //[3]
        res = binder_parse(bs, 0, (uintptr_t) readbuf, bwr.read_consumed, func);
        if (res == 0) {
            ALOGE("binder_loop: unexpected reply?!\n");
            break;
        }
        if (res < 0) {
            ALOGE("binder_loop: io error %d %s\n", res, strerror(errno));
            break;
        }
    }
}
```

`[1]` 向驱动写入`BC_ENTER_LOOPER`命令码，告知binder，线程进入loop模式。binder驱动内部维护了一个

​		 looping thread 的计数。通过`BC_ENTER_LOOPER` 和 `BC_EXIT_LOOPER`来增减。

`[2]` 通过`BINDER_WRITE_READ`来完成和binder驱动的数据交换，`bwr.write_buffer == NULL`,因此不需要写入		 数据到binder驱动。

`[3]` 调用`binder_parse`来解析数据，并使用`func == svcmgr_handler`来处理解析完成后的数据。



`frameworks/native/cmds/servicemanager/binder.c`

```c++
int binder_parse(struct binder_state *bs, struct binder_io *bio,
                 uintptr_t ptr, size_t size, binder_handler func)
{
    int r = 1;
    uintptr_t end = ptr + (uintptr_t) size;

    while (ptr < end) {
        uint32_t cmd = *(uint32_t *) ptr;
        ptr += sizeof(uint32_t);
        switch(cmd) {
        case BR_TRANSACTION: {
            struct binder_transaction_data *txn = (struct binder_transaction_data *) ptr;
            if ((end - ptr) < sizeof(*txn)) {
                ALOGE("parse: txn too small!\n");
                return -1;
            }
            binder_dump_txn(txn);
            if (func) {
                unsigned rdata[256/4];
                struct binder_io msg;
                struct binder_io reply;
                int res;
                bio_init(&reply, rdata, sizeof(rdata), 4);
                bio_init_from_txn(&msg, txn);
                //调用svcmgr_handler解析数据
                res = func(bs, txn, &msg, &reply);
                if (txn->flags & TF_ONE_WAY) {
                    binder_free_buffer(bs, txn->data.ptr.buffer);
                } else {
                    binder_send_reply(bs, &reply, txn->data.ptr.buffer, res);
                }
            }
            ptr += sizeof(*txn);
            break;
        }
    }

    return r;
}
```





至此，ServiceManager进程的主要工作已经很清晰了，其启动后就是处理binder驱动过来的数据。

其主要工作内容如下:

```c+=
enum {
    /* Must match definitions in IBinder.h and IServiceManager.h */
    PING_TRANSACTION  = B_PACK_CHARS('_','P','N','G'),
    SVC_MGR_GET_SERVICE = 1,
    SVC_MGR_CHECK_SERVICE,
    SVC_MGR_ADD_SERVICE,
    SVC_MGR_LIST_SERVICES,
};
```



这里暂不分析其实如何管理其他服务的。

如果我们需要通过ServiceManager获取服务，应该怎么做？



# IServiceManager

> 需要了解IBinder BBinder BpBinder ProcessState IPCThreadState等类的原理。



如果我们需要从ServiceManager中拿到我们需要的服务，就需要通过ServiceManager的代理类来获取。在Native层，其代理类实现在如下：

`frameworks/native/libs/binder/IServiceManager.cpp`

```c++
class IServiceManager : public IInterface
{
public:
    DECLARE_META_INTERFACE(ServiceManager);

    /**
     * Retrieve an existing service, blocking for a few seconds
     * if it doesn't yet exist.
     */
    virtual sp<IBinder>         getService( const String16& name) const = 0;

    /**
     * Retrieve an existing service, non-blocking.
     */
    virtual sp<IBinder>         checkService( const String16& name) const = 0;

    /**
     * Register a service.
     */
    virtual status_t            addService( const String16& name,
                                            const sp<IBinder>& service,
                                            bool allowIsolated = false) = 0;

    /**
     * Return list of all existing services.
     */
    virtual Vector<String16>    listServices() = 0;

    enum {
        GET_SERVICE_TRANSACTION = IBinder::FIRST_CALL_TRANSACTION,
        CHECK_SERVICE_TRANSACTION,
        ADD_SERVICE_TRANSACTION,
        LIST_SERVICES_TRANSACTION,
    };
};

sp<IServiceManager> defaultServiceManager();

```







## defaultServiceManager

该函数用于获取到ServiceManager实例。看看他是怎么拿到serviec_manager进程在binder驱动中的代表的。

```c++
//其实现可精简为
return interface_cast<IServiceManager>(ProcessState::self()->getContextObject(NULL));

//结合ProcessState可进一步简化为
return interface_cast<IServiceManager>(new BpBinder(0));
```

`interface_cast`将一个`BpBinder`指针转换成`sp<IServiceManager>`，他是怎么做到的？

```c++
template<typename INTERFACE>
inline sp<INTERFACE> interface_cast(const sp<IBinder>& obj)
{
    return INTERFACE::asInterface(obj);
}
```

Oops，最终通过`IServiceManager::asInterface`将`BpBinder`指针转换成`sp<IServiceManager>`。

```c++
   android::sp<IServiceManager> IServiceManager::asInterface(                
            const android::sp<android::IBinder>& obj)                   
    {                                                                   
        android::sp<IServiceManager> intr;                                 
        if (obj != NULL) {           
            //又回到了BpBinder->queryLocalInterface,返回值恒为NULL
            intr = static_cast<IServiceManager*>(                          
                obj->queryLocalInterface(                               
                        IServiceManager::descriptor).get());               
            if (intr == NULL) {                                         
                intr = new BpServiceManager(obj);                          
            }                                                           
        }                                                               
        return intr;                                                    
    }          
```



so，最终会创建一个`BpServiceManager`对象











`BINDER_LOOPER_STATE_ENTERED` 和 `BC_REGISTER_LOOPER` 的区别：

```c++
case BC_REGISTER_LOOPER:
			binder_debug(BINDER_DEBUG_THREADS,
				     "%d:%d BC_REGISTER_LOOPER\n",
				     proc->pid, thread->pid);
			binder_inner_proc_lock(proc);
			if (thread->looper & BINDER_LOOPER_STATE_ENTERED) {
				thread->looper |= BINDER_LOOPER_STATE_INVALID;
				binder_user_error("%d:%d ERROR: BC_REGISTER_LOOPER called after BC_ENTER_LOOPER\n",
					proc->pid, thread->pid);
			} else if (proc->requested_threads == 0) {
				thread->looper |= BINDER_LOOPER_STATE_INVALID;
				binder_user_error("%d:%d ERROR: BC_REGISTER_LOOPER called without request\n",
					proc->pid, thread->pid);
			} else {
				proc->requested_threads--;
				proc->requested_threads_started++;
			}
			thread->looper |= BINDER_LOOPER_STATE_REGISTERED;
			binder_inner_proc_unlock(proc);
			break;
		case BC_ENTER_LOOPER:
			binder_debug(BINDER_DEBUG_THREADS,
				     "%d:%d BC_ENTER_LOOPER\n",
				     proc->pid, thread->pid);
			if (thread->looper & BINDER_LOOPER_STATE_REGISTERED) {
				thread->looper |= BINDER_LOOPER_STATE_INVALID;
				binder_user_error("%d:%d ERROR: BC_ENTER_LOOPER called after BC_REGISTER_LOOPER\n",
					proc->pid, thread->pid);
			}
			thread->looper |= BINDER_LOOPER_STATE_ENTERED;
			break;
```









直接看native层的实现：





看看`binder_loop`的工作:

`frameworks/native/cmds/servicemanager/binder.c`

```c++
void binder_loop(struct binder_state *bs, binder_handler func)
{
    int res;
    struct binder_write_read bwr;
    uint32_t readbuf[32];

    bwr.write_size = 0;
    bwr.write_consumed = 0;
    bwr.write_buffer = 0;

    //Register a spawned looper thread with the device.   内核注释。
    readbuf[0] = BC_ENTER_LOOPER;
    binder_write(bs, readbuf, sizeof(uint32_t));

    for (;;) {
        bwr.read_size = sizeof(readbuf);
        bwr.read_consumed = 0;
        bwr.read_buffer = (uintptr_t) readbuf;

        //从驱动获取进程需要处理的任务
        res = ioctl(bs->fd, BINDER_WRITE_READ, &bwr);

        if (res < 0) {
            ALOGE("binder_loop: ioctl failed (%s)\n", strerror(errno));
            break;
        }

        //解析并处理binder数据
        res = binder_parse(bs, 0, (uintptr_t) readbuf, bwr.read_consumed, func);
        if (res == 0) {
            ALOGE("binder_loop: unexpected reply?!\n");
            break;
        }
        if (res < 0) {
            ALOGE("binder_loop: io error %d %s\n", res, strerror(errno));
            break;
        }
    }
}
```



上面的`func` 即 `svcmgr_handler`

```c++

int svcmgr_handler(struct binder_state *bs,
                   struct binder_transaction_data *txn,
                   struct binder_io *msg,
                   struct binder_io *reply)
{
    struct svcinfo *si;
    uint16_t *s;
    size_t len;
    uint32_t handle;
    uint32_t strict_policy;
    int allow_isolated;

	
    if (txn->target.ptr != BINDER_SERVICE_MANAGER)
        return -1;

    if (txn->code == PING_TRANSACTION)
        return 0;

    // Equivalent to Parcel::enforceInterface(), reading the RPC
    // header with the strict mode policy mask and the interface name.
    // Note that we ignore the strict_policy and don't propagate it
    // further (since we do no outbound RPCs anyway).
    strict_policy = bio_get_uint32(msg);
    s = bio_get_string16(msg, &len);
    if (s == NULL) {
        return -1;
    }

    if ((len != (sizeof(svcmgr_id) / 2)) ||
        memcmp(svcmgr_id, s, sizeof(svcmgr_id))) {
        fprintf(stderr,"invalid id %s\n", str8(s, len));
        return -1;
    }


    switch(txn->code) {
    case SVC_MGR_GET_SERVICE:
    case SVC_MGR_CHECK_SERVICE:
        s = bio_get_string16(msg, &len);
        if (s == NULL) {
            return -1;
        }
        handle = do_find_service(s, len, txn->sender_euid, txn->sender_pid);
        if (!handle)
            break;
        bio_put_ref(reply, handle);
        return 0;

    case SVC_MGR_ADD_SERVICE:
        s = bio_get_string16(msg, &len);
        if (s == NULL) {
            return -1;
        }
        handle = bio_get_ref(msg);
        allow_isolated = bio_get_uint32(msg) ? 1 : 0;
        if (do_add_service(bs, s, len, handle, txn->sender_euid,
            allow_isolated, txn->sender_pid))
            return -1;
        break;

    case SVC_MGR_LIST_SERVICES: {
        uint32_t n = bio_get_uint32(msg);

        if (!svc_can_list(txn->sender_pid, txn->sender_euid)) {
            ALOGE("list_service() uid=%d - PERMISSION DENIED\n",
                    txn->sender_euid);
            return -1;
        }
        si = svclist;
        while ((n-- > 0) && si)
            si = si->next;
        if (si) {
            bio_put_string16(reply, si->name);
            return 0;
        }
        return -1;
    }
    default:
        ALOGE("unknown code %d\n", txn->code);
        return -1;
    }

    bio_put_uint32(reply, 0);
    return 0;
}

```





根据`code`字段确定`binder_transaction_data`传输的内容到底是什么。

```c++
struct binder_transaction_data {
	/* The first two are only used for bcTRANSACTION and brTRANSACTION,
	 * identifying the target and contents of the transaction.
	 */
	union {
		/* target descriptor of command transaction */
		__u32	handle;
		/* target descriptor of return transaction */
		binder_uintptr_t ptr;
	} target;
	binder_uintptr_t	cookie;	/* target object cookie */
	__u32		code;		/* transaction command */

	/* General information about the transaction. */
	__u32	        flags;
	pid_t		sender_pid;
	uid_t		sender_euid;
	binder_size_t	data_size;	/* number of bytes of data */
	binder_size_t	offsets_size;	/* number of bytes of offsets */

	/* If this transaction is inline, the data immediately
	 * follows here; otherwise, it ends with a pointer to
	 * the data buffer.
	 */
	union {
		struct {
			/* transaction data */
			binder_uintptr_t	buffer;
			/* offsets from buffer to flat_binder_object structs */
			binder_uintptr_t	offsets;
		} ptr;
		__u8	buf[8];
	} data;
};
```






![1576228320424](images/1576228320424.png)

![img](images/1952665-2bd99d4dfde54836.jpeg)