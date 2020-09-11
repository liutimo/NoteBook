---
title: Android Binder -- AIDL 原理
permalink: Java Binder
date: 2020-09-10 12:49:22
tags: 
	- Android
	- Binder
categories:	
	- Android Framework
---



# Android Binder -- AIDL 原理

要理解AIDL的原理，其实很简单，就是不通过AIDL完成一次Binder IPC调用。

# 一个简单的实例

> 我们创建一个Binder对象并将其添加到ServiceManager中，并且在另外一个进程中引用。
>
> 需要对binder机制有一定了解。

## 定义通用接口

和Native Binder类似，要使用Binder，我们就要定义一个继承了`IInterface`的接口，并在其中定义`code`和`descriptor`以及声明我们的IPC函数。

```kotlin
import android.os.Binder
import android.os.IInterface

interface ICustomService : IInterface {
    companion object {
        const val TRANSACTON_GETPID = Binder.FIRST_CALL_TRANSACTION + 0
        const val descriptor = "com.github.service.ICustomService"
    }
    fun getPid(): Int;
}
```

接下来，无论是Binder实体类还是Binder代理类，都需要实现该接口。

## IPC服务提供者

### 定义Binder实体类

对于Binder实体类，除了继承之前定义的`ICustomService`还要继承`Binder`并且实现它的`onTransact`:

根据`code`的不同，执行不同的逻辑。

> 这里定义为抽象类，所以后续还要继承该类并实现 未实现的方法
>
> 也可以直接在这里实现 所有方法。后续直接创建 对象并添加到ServiceManger中

```kotlin
import android.os.Binder
import android.os.IBinder
import android.os.Parcel

abstract class BnCustomService : Binder(), ICustomService {

    override fun asBinder(): IBinder = this

    override fun onTransact(code: Int, data: Parcel, reply: Parcel?, flags: Int): Boolean {
        when (code) {
            ICustomService.TRANSACTON_GETPID -> {
                data.enforceInterface(ICustomService.descriptor)
                reply?.writeInt(getPid())
                return true
            }
            INTERFACE_TRANSACTION -> {
                reply?.writeString(ICustomService.descriptor)
                return true
            }
            else -> {
                return super.onTransact(code, data, reply, flags)
            }
        }
    }
}
```



### 将Binder实体对象添加到ServiceManager

首先我们要为`BnCustomService`实例化一个对象，由于其实抽象类，所以我们定义了一个匿名类并在其中实现了`getPid`方法。

然后调用`ServiceManager.addService`将其添加到`ServiceManager`中。

```kotlin
import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.os.Process
import android.os.ServiceManager

class MainActivity : AppCompatActivity() {
    private val mBinder = object: BnCustomService() {
        override fun getPid(): Int {
            return Process.myPid()
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
    }

    override fun onStart() {
        super.onStart()
        ServiceManager.addService("liutimo", mBinder)
    }
}

```



## IPC服务消费者

同样需要继承`ICustomService`。

```kotlin
import android.os.IBinder
import android.os.Parcel

class BpCustomService(val remote: IBinder) : ICustomService {

    override fun getPid(): Int {
        val data = Parcel.obtain()
        val reply = Parcel.obtain()
        data.writeInterfaceToken(ICustomService.descriptor)
        remote.transact(ICustomService.TRANSACTON_GETPID, data, reply, 0)
        val pid = reply.readInt()
        data.recycle()
        reply.recycle()
        return pid
    }

    override fun asBinder(): IBinder {
        return remote
    }
}
```

和前面不同的是，这里持有了一个`IBinder`对象。该对象通过`ServiceManager.getService`获得。和Native Binder中的`BpRefBase`和`BpBinder`是一样的。

然后，就可以获取服务并执行IPC函数了。

```kotlin
class MainActivity : AppCompatActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
    }
    fun onClick(view: View) {
        when (view.id) {
            R.id.button -> {
                val binder = ServiceManager.getService("liutimo")
                val proxy = CustomServiceProxy(binder)
                Toast.makeText(this, "${proxy.getPid()}", Toast.LENGTH_SHORT).show()
            }
            else -> {
            }
        }
    }
}
```



# 总结

大家可以看一下`aidl`文件编译后生成的`class`文件，基本上就是上面这些流程，不过是将我们的`ICustomService`、`BnCustomService`和`BpCustomService`改了名字并且放在一个文件里面。还额外提供了一个`asInterface`函数来完成我们上面的`val proxy = CustomServiceProxy(binder)`操作。

使用AIDL的好处就是我们可以省略上述的一些繁琐的通信过程(这些AIDL都自动帮我们完成了)，只需要关注接口的实现即可。