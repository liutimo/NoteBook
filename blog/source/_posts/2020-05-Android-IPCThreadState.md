---
title: Android-IPCThreadState
permalink: Android-IPCThreadState
date: 2020-05-20 18:58:50
categories: Android Framework
tags: 
    - Android Framework
    - Binder
---



读过`ProcessState`的代码，我们知道，进程通过`ProcessState`的构造函数打开`Binder`驱动，通过`ProcessState::startThreadPool`创建一个线程，这个线程会调用`IPCThreadState::self()->joinThreadPool(mIsMain);`l

此时，我们还不清楚其作用，但是可以猜测，这个线程的作用就是同binder驱动进行数据交互了。



## IPCThreadState 构造函数

```c++
IPCThreadState::IPCThreadState()
    : mProcess(ProcessState::self()),
      mMyThreadId(gettid()),
      mStrictModePolicy(0),
      mLastTransactionBinderFlags(0)
{
    //每个线程一个IPCThreadState对象
    pthread_setspecific(gTLS, this);
    clearCaller();
    mIn.setDataCapacity(256);
    mOut.setDataCapacity(256);
}
```

这里，除了初始化`mProecess`数据成员之外，还有一个就是`pthread_setspecific`。

按照每个进程一个`PorcessState`，每个线程一个`IPCThreadState`的逻辑，那么，这里用到`pthread_key_t`也就是很正常不过的。