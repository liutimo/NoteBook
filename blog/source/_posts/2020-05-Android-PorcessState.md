---
title: Android_PorcessState
permalink: Android_PorcessState
date: 2020-05-18 18:34:43
categories: Android Framework
tags: 
    - Android Framework
    - Binder
---

Android 为native 使用binder提供了一套接口。通过这套接口，我们能够比较简单是获取其他服务使用其他服务提供的功能。即使其他服务是Java进程也可以轻松访问。

例如，我们需要使用C++编写一个android关机程序，通常，我们会直接调用`system`函数执行`reboot -p`命令。虽然功能上是实现了，但是实现过程却不完美。最好的方式就是获取`PowerManager`提供的服务。

我们首先通过`ServiceManager`获取到`PowerMannager`服务，然后调用`PowerManager.shutdown`完成关机操作。

```c++
android::sp<android::ProcessState> ps(android::ProcessState::self());
ps->startThreadPool();

const auto sm = android::defaultServiceManager();
android::sp<android::IPowerManager> service = android::interface_cast<android::IPowerManager>(
                                                sm->getService(android::String16("power")));
service->shutdown(false, android::String16("mcu"), false); 
```

向这样，就能够直接在native代码中完成关机操作，而不需要去通过jni调用java方法完成。



## ProcessState()

先看构造函数。

```c++
ProcessState::ProcessState(const char *driver)
    : mDriverName(String8(driver))
    , mDriverFD(open_driver(driver))
    , mVMStart(MAP_FAILED)
    , mThreadCountLock(PTHREAD_MUTEX_INITIALIZER)
    , mThreadCountDecrement(PTHREAD_COND_INITIALIZER)
    , mExecutingThreadsCount(0)
    , mMaxThreads(DEFAULT_MAX_BINDER_THREADS)
    , mStarvationStartTimeMs(0)
    , mManagesContexts(false)
    , mBinderContextCheckFunc(NULL)
    , mBinderContextUserData(NULL)
    , mThreadPoolStarted(false)
    , mThreadPoolSeq(1)
{
    if (mDriverFD >= 0) {
        // mmap the binder, providing a chunk of virtual address space to receive transactions.
        mVMStart = mmap(0, BINDER_VM_SIZE, PROT_READ, MAP_PRIVATE | MAP_NORESERVE, mDriverFD, 0);
    }
}
```

构造函数的工作就是打开binder驱动并完成`mmap`。so easy。

接下来，我们调用了`startThreadPool`函数，看看他做了什么？

```c++
void ProcessState::startThreadPool()
{
    AutoMutex _l(mLock);
    if (!mThreadPoolStarted) {
        mThreadPoolStarted = true;
        spawnPooledThread(true);
    }
}

void ProcessState::spawnPooledThread(bool isMain)
{
    if (mThreadPoolStarted) {
        String8 name = makeBinderThreadName();
        sp<Thread> t = new PoolThread(isMain);
        t->run(name.string());
    }
}
```

`startThreadPool`函数执行时，创建一个`PoolThread`的线程。

```c++
class PoolThread : public Thread
{
public:
    explicit PoolThread(bool isMain)
        : mIsMain(isMain)
    {
    }
protected:
    virtual bool threadLoop()
    {
        IPCThreadState::self()->joinThreadPool(mIsMain);
        return false;
    }
    const bool mIsMain;
};
```

最终调用`IPCThreadState::self()->joinThreadPool(mIsMain)`完成子线程的处理逻辑。

其完成的内容具体到`IPCThreadState`中分析，这里简单的说明一下，就是其完成了同Binder驱动的数据交互。

所以，在这里，`ProcessState`的作用就是完成了进程和Binder驱动的初始化，并创建了一个同Binder驱动完成数据交互的线程。



## getContextObject()

该函数在`defaultServiceMananger`中被使用。

```c++
sp<IServiceManager> defaultServiceManager()
{
    if (gDefaultServiceManager != NULL) return gDefaultServiceManager;

    {
        AutoMutex _l(gDefaultServiceManagerLock);
        while (gDefaultServiceManager == NULL) {
            //here。
            gDefaultServiceManager = interface_cast<IServiceManager>(
                ProcessState::self()->getContextObject(NULL));
            if (gDefaultServiceManager == NULL)
                sleep(1);
        }
    }
    return gDefaultServiceManager;
}
```

其通过传入一个`NULL`参数，就获取到了我们的`ServiceMananger`服务？how does  it do this？

```c++
sp<IBinder> ProcessState::getContextObject(const sp<IBinder>& /*caller*/)
{
    return getStrongProxyForHandle(0);
}

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

            b = new BpBinder(handle); 
            e->binder = b;
            if (b) e->refs = b->getWeakRefs();
            result = b;
        } else {
            result.force_set(b);
            e->refs->decWeak(this);
        }
    }

    return result;
}
```

`lookupHandleLocked`会根据`handle`来找到相应的`handle_entry`，如果不存在，则会构建一个空的`handle_entry`.

```c++
ProcessState::handle_entry* ProcessState::lookupHandleLocked(int32_t handle)
{
    const size_t N=mHandleToObject.size();
    if (N <= (size_t)handle) {
        handle_entry e;
        //注意这个咯。
        e.binder = NULL;
        e.refs = NULL;
        status_t err = mHandleToObject.insertAt(e, N, handle+1-N);
        if (err < NO_ERROR) return NULL;
    }
    return &mHandleToObject.editItemAt(handle);
}
```

最后，在`getStrongProxyForHandle`中，会进入如下代码：

```c++
if (b == NULL || !e->refs->attemptIncWeak(this)) {
	if (handle == 0) {
		Parcel data;
		status_t status = IPCThreadState::self()->transact(
							0, IBinder::PING_TRANSACTION, data, NULL, 0);
		if (status == DEAD_OBJECT)
            return NULL;
       }
    b = new BpBinder(handle); 
    e->binder = b;
    if (b) e->refs = b->getWeakRefs();
   result = b;
} else {
    result.force_set(b);
    e->refs->decWeak(this);
}	
```

> 对于handle == 0的，其总是指向ServiceMananger

以`handle == 0`为例，这里创建了一个`BpBinder`对象。`BpBinder`表示的是一个代理对象，这里表示的是ServiceMananger在当前进程的代理。通过这个代理，我们就能完成和`ServuceMananger`进程的通信。



另外比较重要的`interface_cast`见<待补充>。

> 此外，`getContextObject`还有一个重载版本，暂且忽略不看。



