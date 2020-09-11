---
title: ActivityManagerService
permalink: ActivityManagerService
date: 2020-09-07 22:40:25
tags:
categories:
---





# AMS的启动



## SystemServer

AMS在`SystemServer`中启动！启动流程如下：

`SystemServer::run()` => `SystemServer::startBootstrapServices()`

![image-20200907224435908](images/image-20200907224435908.png)

这里通过`SystemServiceManager`来启动`AMS`。

## SystemServiceManager

![image-20200907224923635](images/image-20200907224923635.png)

这里可以看到，要通过`SystemServiceManager`来启动`Service`，`Service`就必须继承`SystemService`类。

启动`AMS`时，首选就校验其是否是`SystemService`的子类，随后通过反射来获取构造一个对象。

这个对象就是`ActivityManangerService.Lifecycle`。

`startService`的作用就是将其添加到一个`service`数组`SystemServiceManager::mServices`中。随后调用`SystemService::onStart()`方法：

![image-20200907225311538](images/image-20200907225311538.png)



## ActivityManagerService::Lifecycle

​	![image-20200907225754421](images/image-20200907225754421.png)

这是什么设计模式？？`Lifecycle`作为`ActivityManangerService`的内部类。

上面的服务启动过程就可以理解为创建一个`ActivityManagerService`对象并调用其`start`方法。

## ActivityManagerService::start

![image-20200907230256378](images/image-20200907230256378.png)]

`publish`作为`service`的成员函数，其内部实现相当于：

```java
 ServiceManager.addService(Context.APP_OPS_SERVICE, asBinder());
```

拥有该函数的`service`都继承至`Stub`。所以`publish`的作用就是注册服务。

这里注册了`BatteryStatsService`、`AppOpsService`。



