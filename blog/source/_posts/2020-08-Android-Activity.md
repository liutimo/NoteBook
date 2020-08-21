---
title: Android Activity
permalink: Android Activity
date: 2020-08-17 16:22:44
tags: - Android 
  	- Activity
categories: Android
---



## Activity的生命周期



### 通常情况下的生命周期

![img](images/activity_lifecycle.png)



`onCreate`: 完成初始化操作。

`onStart`:此时Activity已经可见，但还没有出现在前台，无法和用户交互。这个时候其实可以理解为Activity已经显示出来了，但是我们还看不到。

`onResume`: 表示Activity已经可见了，并且出现在前台并开始活动。要注意这个和onStart的对比，onStart和onResume都表示Activity已经可见，但是onStart的时候Activity还在后台，onResume的时候Activity才显示到前台。

`onPause`: Activity正在停止。此时可以做一些存储数据、停止动画等工作，但是注意不能太耗时，因为这会影响到新Activity的显示，onPause必须先执行完，新Activity的onResume才会执行。

`onStop`: :yellow_heart: 也不能执行太耗时的操作。

`onDestroy`：表示Activity即将被销毁，这是Activity生命周期中的最后一个回调，在这里，我们可以做一些回收工作和最终的资源释放。

`onRestart`：表示Activity正在重新启动。一般情况下，当当前Activity从不可见重新变为可见状态时，onRestart就会被调用。这种情形一般是用户行为所导致的，比如用户按Home键切换到桌面或者用户打开了一个新的Activity，这时当前的Activity就会暂停，也就是onPause和onStop被执行了，接着用户又回到了这个Activity，就会出现这种情况。



### 异常情况下的生命周期

#### 资源相关的系统配置导致Activity重启

以屏幕旋转为例，其会导致Activity被销毁并重新创建。此时，`onPause`、`onStop`和`onDestory`均会被调用。此外，系统还会调用一个`onSaveInstanceState`的函数来保存Activity的状态。其调用在`onStop`之后 `onDestory`之前。

通过配置`AndroidMainfest.xml`中给`activity`添加`android:configChanges="orientation"`属性，可以阻止activity被销毁。

![image-20200820223043198](images/image-20200820223043198.png)



使用方式:

```xml
<activity
        android:name="com.ryg.chapter_1.MainActivity"
        android:configChanges="orientation|screenSize"
        android:label="@string/app_name" >
        <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
        </intent-filter>
    </activity>
```





#### 资源内存不足导致低优先级的Activity被杀死

Activity的优先级如下：

1. 前台Activity

   正在和用户交互的Activity，优先级最高。

2. 可见但非前台Activity

   比如Activity中弹出了一个对话框，导致Activity可见但是位于后台无法和用户直接交互。

3. 后台Activity

   已经被暂停的Activity，比如执行了onStop，优先级最低。





## Acitvity的启动模式



Android提供了4种启动模式。

1. Standard

   默认模式，每次启动时创建一个新的实例，无论当前Activity是否已存在多个实例。standard模式的Activity默认会进入启动它的Activity所属的任务栈中，但是由于非Activity类型的Context（如ApplicationContext）并没有所谓的任务栈，所以这就有问题了。解决这个问题的方法是为待启动Activity指定FLAG_ACTIVITY_NEW_TASK标记位，这样启动的时候就会为它创建一个新的任务栈，这个时候待启动Activity实际上是以singleTask模式启动的。

2. SingleTop

   栈顶复用模式。如果新的Activity已经有一个实例位于栈顶，就不在重新创建。此外，`onCreate`、`onStart`不会被调用， 但是会依次调用`onNewIntent`、`onResume`。

Standard,  singleTop,  singleTask,  singleInstance



## IntentFilter



