# Looper

用于在线程中创建消息循环。线程默认没有和message loop相关联,并且一个线程只能拥有一个`Looper`对象。

当线程使用Looper创建消息循环时，需要调用`Looper.prepare()`，然后再调用`Looper.loop()`。

eg:

```java
class LooperThread extends Thread {
    public Handler mHandler;
    
    public void run() {
       Looper.prepare();
        mHandler = new Handler() {
           public void handleMessage(Message msg) {
               //do
           } 
        };
        Looper.loop();
    }

```

从`Handler`的源码中，可以看到，如果Looper初始化错误，Handler就会创建失败。

```java
public Handler(Callback callback, boolean async) {
	...
	mLooper = Looper.myLooper();
 	if (mLooper == null) {
		throw new RuntimeException("Can't create handler inside thread that has not called Looper.prepare()");
    }
    ...
}
```



## prepare()

`prepare()`创建了一个线程本地变量`sThreadLocal`来保存当前线程的`Looper`对象，所以每个线程(主线程除外)

再使用`Handler`（通过`Looper.myLooper()`获取当前线程的`Looper`对象）前都需要调用`prepare()`方法。



```java
public static void prepare() {
        prepare(true);
}

private static void prepare(boolean quitAllowed) {
        if (sThreadLocal.get() != null) {
              throw new RuntimeException("Only one Looper may be created per thread");
        }
        sThreadLocal.set(new Looper(quitAllowed));
}
```



## loop()

处理消息队列咯，如果要退出，就需要调用`quit()`函数(不在同一个线程调用)。

精简后的`loop()`实现如下：

```java
public static void loop() {
		final Looper me = myLooper();
		final MessageQueue queue = me.mQueue;
		for (;;) {
				Message msg = queue.next(); // might block
				if (msg == null) {
						return; // No message indicates that the message queue is quitting.
				}
				msg.target.dispatchMessage(msg);
				msg.recycleUnchecked();
		}
}
```

该函数的主要工作就是消息对列中，取出消息并处理。

函数最终调用了`Handler.dispatchMessage`来处理消息。

```java
    public void dispatchMessage(Message msg) {
        if (msg.callback != null) {
            handleCallback(msg);
        } else {
            if (mCallback != null) {
                if (mCallback.handleMessage(msg)) {
                    return;
                }
            }
            handleMessage(msg);
        }
    }
```

可见，大多数情况下，我们继承`Handler`就需要重写`handleMessage`来处理`Message`。



## quit()

```java
	//unsafe，可能会导致某些message在looper退出后不会被交付到指定的handler。
    public void quit() {
        mQueue.quit(false); //清空mQueue
    }

	//保证looper推出前message能被成功交付。defer的message不做保证。
    public void quitSafely() {
        mQueue.quit(true);    
    }

// mQuitting = ture
```

quit后，Looper是如何推出循环的？

```java
//MessageQueue
Message next() {
    ...
        for (;;) {
            ...
            //再次之前，会将所有在当前时间之前的message都分发到指定Handler
           if (mQuitting) {
                dispose();
                return null;
            }
             ...
        }
    ...
}

//next返回null，回到Looper.loop()。oh,原来如此。
```





# HandlerThread

如前面所说，使用Thread内部使用`Handler`需要调用`Looper.prepare()`和`Looper.loop()`。而`HandlerThread`则将这部分操作进行了封装。

```java
public void run() {
    mTid = Process.myTid();
    Looper.prepare();
    synchronized (this) {
        mLooper = Looper.myLooper();
        notifyAll(); //调用getLooper时需要保证线程已经执行到了这一步
    }
    Process.setThreadPriority(mPriority);
    onLooperPrepared();
    Looper.loop();
    mTid = -1;
}
```

其实现很简单，就不多说明了。

