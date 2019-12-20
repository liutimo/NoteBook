# ProcessState

顾名思义，`ProcessState`表示的就是进程的状态信息，因此其实现为单例模式。

`frameworks/native/libs/binder/ProcessState.cpp`



先看看其头文件:

`frameworks/native/include/binder/ProcessState.h`

```c++

class ProcessState : public virtual RefBase
{
public:
    static  sp<ProcessState>    self();

            void                setContextObject(const sp<IBinder>& object);
            sp<IBinder>         getContextObject(const sp<IBinder>& caller);
        
            void                setContextObject(const sp<IBinder>& object,
                                                 const String16& name);
            sp<IBinder>         getContextObject(const String16& name,
                                                 const sp<IBinder>& caller);

            sp<IBinder>         getStrongProxyForHandle(int32_t handle);
            wp<IBinder>         getWeakProxyForHandle(int32_t handle);
private:
    friend class IPCThreadState;
    
                                ProcessState();
                                ~ProcessState();
            struct handle_entry {
                IBinder* binder;
                RefBase::weakref_type* refs;
            };
			Vector<handle_entry>mHandleToObject;
            handle_entry*       lookupHandleLocked(int32_t handle);

            int                 mDriverFD;
            void*               mVMStart;
};
```

我们目前只关心这几个函数的作用。



## 构造函数

构造函数打开了binder驱动，并完成了内存地址map。一个进程只打开一次Binder驱动，

```c++

ProcessState::ProcessState()
    : mDriverFD(open_driver())
    , mVMStart(MAP_FAILED)
{
    if (mDriverFD >= 0) {
        // mmap the binder, providing a chunk of virtual address space to receive transactions.
        mVMStart = mmap(0, BINDER_VM_SIZE, PROT_READ, MAP_PRIVATE | MAP_NORESERVE, mDriverFD, 0);
        if (mVMStart == MAP_FAILED) {
            // *sigh*
            ALOGE("Using /dev/binder failed: unable to mmap transaction memory.\n");
            close(mDriverFD);
            mDriverFD = -1;
        }
    }
}

sp<ProcessState> ProcessState::self()
{
    Mutex::Autolock _l(gProcessMutex);
    if (gProcess != NULL) {
        return gProcess;
    }
    gProcess = new ProcessState;
    return gProcess;
}
```





## getContextObject

在函数`defaultServiceManager`中，使用了该方法，用于获取Service Manager的代理对象。

```c++
interface_cast<IServiceManager>(ProcessState::self()->getContextObject(NULL));
```

先看看其实现：

```c++
sp<IBinder> ProcessState::getContextObject(const sp<IBinder>& /*caller*/)
{
    return getStrongProxyForHandle(0);
}
```

最终还是调用了`getStrongProxyForHandle`





## lookupHandleLocke



```c++
ProcessState::handle_entry* ProcessState::lookupHandleLocked(int32_t handle)
{
    const size_t N = mHandleToObject.size();
    if (N <= (size_t)handle) {
        handle_entry e;
        e.binder = NULL;
        e.refs = NULL;
        status_t err = mHandleToObject.insertAt(e, N, handle+1-N);
        if (err < NO_ERROR) return NULL;
    }
    return &mHandleToObject.editItemAt(handle);
}

```





## getStrongProxyForHandle

顾名思义，头很大。大概就是根据handle 来获取一个强引用的代理对象。

```C++
sp<IBinder> ProcessState::getStrongProxyForHandle(int32_t handle)
{
    sp<IBinder> result;

    AutoMutex _l(mLock);

    handle_entry* e = lookupHandleLocked(handle);

    if (e != NULL) {
        IBinder* b = e->binder;
        if (b == NULL || !e->refs->attemptIncWeak(this)) {
            if (handle == 0) {
                Parcel data;
                status_t status = IPCThreadState::self()->transact(
                        0, IBinder::PING_TRANSACTION, data, NULL, 0);
                if (status == DEAD_OBJECT)
                   return NULL;
            }
			//创建一个BpBinder
            b = new BpBinder(handle); 
            e->binder = b;
            if (b) e->refs = b->getWeakRefs();
            result = b;
        } else {
            // This little bit of nastyness is to allow us to add a primary
            // reference to the remote proxy when this team doesn't have one
            // but another team is sending the handle to us.
            result.force_set(b);
            e->refs->decWeak(this);
        }
    }

    return result;
}

```

首先从`mHandleToObject`中查找`handle_entry`实体，然后可能会创建一个`BpBinder`对象（如果不存在的话）。

先去看`BpBinder`是什么鬼吧。