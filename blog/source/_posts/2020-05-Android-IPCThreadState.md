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

