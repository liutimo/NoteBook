在Android中，存在很多Service，当有app请求service时，如何确定请求的service是哪一个呢？

这就需要一个东西来管理这些service。

于是，ServiceManager出现了。在service启动时，为其制定一个唯一标志符，然后将其注册到ServiceManager。此后，需要使用该service的app就可以通过这个唯一标志符来获取服务。

其流程图如下：

![img](images/f5fd64364d548a08bd1f601f64fb1add.png)



## service manager的实现

service manager的核心工作就是管理系统的service。这个工作并不复杂，理解service manager的关键就是熟悉Binder。其主要代码量感觉都是在解析Binder传来的数据。

先看一下main函数：

> 代码都忽略掉selinux相关的。

```c++
int main(int argc, char** argv)
{
    struct binder_state *bs;
    union selinux_callback cb;
    char *driver;
	//android O 以后出现了三个binder节点，根据实际情况选择合适的binder 节点咯
    if (argc > 1) {
        driver = argv[1];
    } else {
        driver = "/dev/binder";
    }

    //[1] 打开Binder驱动，申请128K的内存空间用户mmap
    bs = binder_open(driver, 128*1024);
    //[2] 设置成context
    binder_become_context_manager(bs);
    //[3]
    binder_loop(bs, svcmgr_handler);
    return 0;
}

```

`[1]` `[2]` 这两处代码没啥可说的，除非去看驱动底层的实现。其功能就是打开驱动，映射内存并设置为全局唯一的context。

`[3]` `binder_loop`，可以看看，其完成同Binder驱动的数据交互。`svcmgr_handler`是一个回调，它就是service manager核心。



### binder_loop

```c++

void binder_loop(struct binder_state *bs, binder_handler func)
{
    int res;
    struct binder_write_read bwr;
    uint32_t readbuf[32];

    bwr.write_size = 0;
    bwr.write_consumed = 0;
    bwr.write_buffer = 0;

    readbuf[0] = BC_ENTER_LOOPER;
    //[1] 告知驱动，进入循环
    binder_write(bs, readbuf, sizeof(uint32_t));

    for (;;) {
        bwr.read_size = sizeof(readbuf);
        bwr.read_consumed = 0;
        bwr.read_buffer = (uintptr_t) readbuf;
        //读数据
        res = ioctl(bs->fd, BINDER_WRITE_READ, &bwr);
        //[2] 解析数据，必要时需要reply
        res = binder_parse(bs, 0, (uintptr_t) readbuf, bwr.read_consumed, func);
    }
}
```

这段代码也没啥特别的，就是通过`ioctl`完成和binder驱动的数据交换。最终还是调用`binder_parse`来解析数据。。。

### binder_parse

```c++
int binder_parse(struct binder_state *bs, struct binder_io *bio,
                 uintptr_t ptr, size_t size, binder_handler func)
{
    //uintptr_t actual type is long unsigned int
    //POSIX 64bit  size is  8bytes
    //ptr实际上是一个整型，其记录着readbuf中当前待处理的元素地址
    int r = 1;
    uintptr_t end = ptr + (uintptr_t) size;

    while (ptr < end) {
        //取值，需要将ptr转化为指针在用*号取值
        uint32_t cmd = *(uint32_t *) ptr;
        //指向下一个元素
        ptr += sizeof(uint32_t);
        switch(cmd) {
        case BR_TRANSACTION: {
            //有传输的数据，binder_transaction_data
            struct binder_transaction_data *txn = (struct binder_transaction_data *) ptr;
            if (func) {
                unsigned rdata[256/4];
                struct binder_io msg;
                struct binder_io reply;
                int res;

                bio_init(&reply, rdata, sizeof(rdata), 4);
                bio_init_from_txn(&msg, txn);
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
        case BR_REPLY: {
            struct binder_transaction_data *txn = (struct binder_transaction_data *) ptr;
            if ((end - ptr) < sizeof(*txn)) {
                ALOGE("parse: reply too small!\n");
                return -1;
            }
            binder_dump_txn(txn);
            if (bio) {
                bio_init_from_txn(bio, txn);
                bio = 0;
            } else {
                /* todo FREE BUFFER */
            }
            ptr += sizeof(*txn);
            r = 0;
            break;
        }
        ...
    }

    return r;
}

```

`ptr`: 其初始值是`uint32_t readbuf[32];`的首地址。在接收数据时，binder驱动会将数据拷贝到`readbuf`中。ptr的使用如下所示，因为`uintptr_t`的实际类型还是整形，并不是一个指针，所以使用时，需要将其转换成指针来操作。

```c++
uint32_t cmd = *(uint32_t*)ptr; 
ptr += sizeof(uint32_t);

struct binder_transaction_data *txn = (struct binder_transaction_data *) ptr;
ptr += sizeof(*txn);
```

看一下binder驱动是如何拷贝`cmd`和`binder_transaction_data`的。

```c++
//省略了很多代码
struct binder_transaction *t = NULL;
struct binder_transaction_data tr;
if (t->buffer->target_node) {
	cmd = BR_TRANSACTION;
} else {
	cmd = BR_REPLY;
}
put_user(cmd, (uint32_t __user *)ptr);
ptr += sizeof(uint32_t);
copy_to_user(ptr, &tr, sizeof(tr));		
ptr += sizeof(tr);
```

原来如此，结合驱动代码来看，就能很快知道，service manager为什么要这么写了。

在解析数据后，就会调用`svcmgr_handler`来处理数据了。

### svcmgr_handler

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
    //target 不是 service_manager
    if (txn->target.ptr != BINDER_SERVICE_MANAGER)
        return -1;

    //PING ignore
    if (txn->code == PING_TRANSACTION)
        return 0;
    strict_policy = bio_get_uint32(msg);
    s = bio_get_string16(msg, &len);
    if ((len != (sizeof(svcmgr_id) / 2)) ||  memcmp(svcmgr_id, s, sizeof(svcmgr_id)));
    
    switch(txn->code) {
    case SVC_MGR_GET_SERVICE:
    case SVC_MGR_CHECK_SERVICE:
        s = bio_get_string16(msg, &len);
        handle = do_find_service(s, len, txn->sender_euid, txn->sender_pid);
        bio_put_ref(reply, handle);
        return 0;

    case SVC_MGR_ADD_SERVICE:
        s = bio_get_string16(msg, &len);
        handle = bio_get_ref(msg);
        allow_isolated = bio_get_uint32(msg) ? 1 : 0;
        do_add_service(bs, s, len, handle, txn->sender_euid, allow_isolated, txn->sender_pid));
        break;

    case SVC_MGR_LIST_SERVICES: {
        uint32_t n = bio_get_uint32(msg);
        svc_can_list(txn->sender_pid, txn->sender_euid));
        si = svclist;
        while ((n-- > 0) && si)
            si = si->next;
        if (si) {
            bio_put_string16(reply, si->name);
            return 0;
        }
        return -1;
    }
    }
    bio_put_uint32(reply, 0);
    return 0;
}
```

service manager 支持如下操作：

```c++
enum {
    /* Must match definitions in IBinder.h and IServiceManager.h */
    PING_TRANSACTION  = B_PACK_CHARS('_','P','N','G'),
    SVC_MGR_GET_SERVICE = 1,
    SVC_MGR_CHECK_SERVICE,
    SVC_MGR_ADD_SERVICE,
    SVC_MGR_LIST_SERVICES,
};
```

`svcmgr_handler`的主要工作就是完成这些操作的逻辑处理。

### service manager如何维护service

service manager通过`svcinfo`来管理注册好的services。

```c++
struct svcinfo
{
    struct svcinfo *next;  	  //链表
    uint32_t handle;			
    struct binder_death death;
    int allow_isolated;		 //判断是否允许孤立进程使用。
    size_t len;				//service name length
    uint16_t name[0];		//service name
};
```

嗯，就是通过单链表来管理的。查找时，通过`memcmp name`来判断是否相等。所以，注册service时，要保证service name的全局唯一性。



### svcmgr_handler的数据解析

在看看其函数声明:

```c++
int svcmgr_handler(struct binder_state *bs,
                   struct binder_transaction_data *txn,
                   struct binder_io *msg,
                   struct binder_io *reply);
```

1. `binder_state`

    ```c++
    struct binder_state
    {
        int fd;
        void *mapped;
        size_t mapsize;
    };
    ```

    记录着进程打开binder驱动的信息。

2. `binder_transaction_data`

    ```c++
struct binder_transaction_data {
    	union {
    		__u32	handle;
    		binder_uintptr_t ptr;
    	} target;
    	binder_uintptr_t	cookie;
    	__u32		code;
    	__u32	        flags;
    	pid_t		sender_pid;
    	uid_t		sender_euid;
    	binder_size_t	data_size;
    	binder_size_t	offsets_size;
    	union {
    		struct {
    			binder_uintptr_t	buffer;
    			binder_uintptr_t	offsets;
    		} ptr;
    		__u8	buf[8];
    	} data;
    };
    ```
    
    
    
    前面的代码中， 其赋值紧跟`cmd`，所以其应该是当前cmd对应的数据。结构体太长，先不分析。
    
    留到binder驱动一节。
    
3. `binder_io`

    这玩意好像只有在service manager的代码中有。。。

    ```c++
    struct binder_io
    {
        char *data;            /* pointer to read/write from */
        binder_size_t *offs;   /* array of offsets */
        size_t data_avail;     /* bytes available in data buffer */
        size_t offs_avail;     /* entries available in offsets array */
    
        char *data0;           /* start of data buffer */
        binder_size_t *offs0;  /* start of offsets buffer */
        uint32_t flags;
        uint32_t unused;
    };
    ```

    慢慢分析其作用吧。

    

    `bio_init_from_txn`，使用`binder_transaction_data`初始化`binder_io`结构。

    ```c++
    void bio_init_from_txn(struct binder_io *bio, struct binder_transaction_data *txn)
    {
        //还不是很清楚binder_transaction_data中buffer和offests之间的关系。边走边看吧
        bio->data = bio->data0 = (char *)(intptr_t)txn->data.ptr.buffer;
        bio->offs = bio->offs0 = (binder_size_t *)(intptr_t)txn->data.ptr.offsets;
        bio->data_avail = txn->data_size;
        bio->offs_avail = txn->offsets_size / sizeof(size_t);
        bio->flags = BIO_F_SHARED;
    }
    ```

    `bio_get`,从`binder_io`中获取指定长度的数据。

    ```c++
    static void *bio_get(struct binder_io *bio, size_t size)
    {
        //四字节对齐
        size = (size + 3) & (~3);
    	//可用数据不足
        if (bio->data_avail < size){
            bio->data_avail = 0;
            bio->flags |= BIO_F_OVERFLOW;
            return NULL;
        }  else {
            void *ptr = bio->data;
            bio->data += size;
            bio->data_avail -= size;
            return ptr;
        }
    }
    ```

    从上面的代码片段可以看出：

    `binder_io->data`就是binder驱动传递上来的数据的缓冲区。而`binder_io_data_avail`就是该缓冲区的大小。

    再来看看`bio_put_ref`：

    ```c++
    bio_put_ref(reply, handle);
    void bio_put_ref(struct binder_io *bio, uint32_t handle)
    {
        struct flat_binder_object *obj;
        if (handle)
            obj = bio_alloc_obj(bio);
        else
            obj = bio_alloc(bio, sizeof(*obj));
        obj->flags = 0x7f | FLAT_BINDER_FLAG_ACCEPTS_FDS;
        obj->type = BINDER_TYPE_HANDLE;
        obj->handle = handle;
        obj->cookie = 0;
    }
    ```

    `bio_alloc_obj`和`bio_alloc`的区别在于，前者有操作`bio->offs_avail`和`bio->offs`。

    ```c++
    static struct flat_binder_object *bio_alloc_obj(struct binder_io *bio)
    {
        struct flat_binder_object *obj;
        obj = bio_alloc(bio, sizeof(*obj));
        if (obj && bio->offs_avail) {
            bio->offs_avail--;
            *bio->offs++ = ((char*) obj) - ((char*) bio->data0);
            return obj;
        }
    }
    ```

    so。每次alloc 一个 `flat_binder_object`，`bio->offs_avail`的值就会减一。那么`bio->offs_avail`的含义是不是就是当前`binder_io`中能够容纳的`flat_binder_object`的个数呢。

    `bio->offs`又是啥呢？

    > 不知道

    毕竟`binder_io`的结构体应该是根据`binder_transaction_data`来构建的。还是要了解`binder_transaction_data`再来看`binder_io`。

    其实，service manager的主要流程已经很清晰了，数据传输感觉还是得先了解binder驱动的原理才能更清晰。

    