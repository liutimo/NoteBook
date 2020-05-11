---
title: Android-MessageQueue
permalink: Android-MessageQueue
date: 2020-05-08 16:57:57
categories: Android Framework
tags: 
    - Android Framework
---





自顶向下看吧。故事还得从`Looper.loop`讲起。

```java
Message msg = queue.next(); // might block
msg.target.dispatchMessage(msg);
```

其核心就是这样，从`MessageQueue`中取出`Message`并调用其回调函数完成消息分发。这篇文章的重点就是`MessageQueue`的实现。

其实现分为Native和Java层。

