---
title: libbinder
permalink: libbinder
date: 2020-08-17 19:56:14
tags:
categories:
---

![img](images/2020-08-libbinder/5297002-ef926e2d52fd756a.PNG)





![img](images/2020-08-libbinder/5b29264100011e7820002018.jpg)













## IBinder

Base class and low-level protocol (底层协议) for a remotable object. You can derive from this class to create an object for which other processes can hold references to it. 

Communication between processes (method calls, property get and set) is down through a low-level protocol implemented on top of the transact() API.

通过实现 `transact()` 完成进程通信(属性设置、获取或方法调用)。



### IBinder中的枚举值

```c++
enum
{
    //这两个用于定义 一个code我们自定义的code。transact()方法的第一个参数
    FIRST_CALL_TRANSACTION = 0x00000001,
    LAST_CALL_TRANSACTION = 0x00ffffff,
	
    //默认的一些code。
    PING_TRANSACTION = B_PACK_CHARS('_', 'P', 'N', 'G'),
    DUMP_TRANSACTION = B_PACK_CHARS('_', 'D', 'M', 'P'),
    SHELL_COMMAND_TRANSACTION = B_PACK_CHARS('_', 'C', 'M', 'D'),
    INTERFACE_TRANSACTION = B_PACK_CHARS('_', 'N', 'T', 'F'),
    SYSPROPS_TRANSACTION = B_PACK_CHARS('_', 'S', 'P', 'R'),

    // Corresponds to TF_ONE_WAY -- an asynchronous call.
    FLAG_ONEWAY = 0x00000001
};
```



### 函数

`IBinder`中大部分函数都需要子类(`BBinder` or `BpBinder`)来实现。其中比较重要的就是 `transact` 方法。

```c++
//后面在BpBinder中会看到他的实现。
virtual status_t transact(uint32_t code,         	
                          const Parcel &data,
                          Parcel *reply,
                          uint32_t flags = 0) = 0;
```







