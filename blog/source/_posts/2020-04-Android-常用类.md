---
title: Android_smart_pointer
date: 2020-05-02 19:23:59
categories: Android Framework
tags: 
    - Android Framework
---

Android并没有直接引用C++标准库的智能指针，而是通过`std::atomic`重新实现了一套智能指针。这么做的原因估计是标准库提供的不够用吧。

Android中的智能指针相关的类主要是`sp<T>`，`wp<T>`，`RefBase`。



## RefBase

`RefBase`在这里面算是一个比较重要的类，所有需要使用`sp`或者`wp`的类都需要继承`RefBase`。

### `weakref_type`

`RefBase`有一个内嵌类`weakref_type`，这个类定义如下:

```c++
class weakref_type
{
public:
	RefBase*            refBase() const;

	void                incWeak(const void* id);
	void                decWeak(const void* id);

	// acquires a strong reference if there is already one.
	bool                attemptIncStrong(const void* id);

	// acquires a weak reference if there is already one.
	// This is not always safe. see ProcessState.cpp and BpBinder.cpp
	// for proper use.
	bool                attemptIncWeak(const void* id);
};
```

`weakref_type`主要实现了和弱引用相关的操作。其有一个子类`weakref_impl`。

#### `weakref_impl`

```c++
class RefBase::weakref_impl : public RefBase::weakref_type
{
public:
    std::atomic<int32_t>    mStrong;
    std::atomic<int32_t>    mWeak;
    RefBase* const          mBase;
    std::atomic<int32_t>    mFlags;

    explicit weakref_impl(RefBase* base)
            : mStrong(INITIAL_STRONG_VALUE)
            , mWeak(0)
            , mBase(base)
            , mFlags(0)
    {
    }

    //无用
    void addStrongRef(const void* /*id*/) { }
    void removeStrongRef(const void* /*id*/) { }
    void renameStrongRefId(const void* /*old_id*/, const void* /*new_id*/) { }
    void addWeakRef(const void* /*id*/) { }
    void removeWeakRef(const void* /*id*/) { }
    void renameWeakRefId(const void* /*old_id*/, const void* /*new_id*/) { }
    void printRefs() const { }
    void trackMe(bool, bool) { }
};
```

`weakref_impl`的4个成员变量左右如下：
    `mStrong`： 强引用计数
    `mWeak`：弱引用计数
    `mBase`：指向相应的`RefBase`
    `mFlags`:标记`RefBase`的生命周期受强引用还是弱引用影响



### 如何操作引用计数

强引用计数增减由`RefBase`来完成。弱引用计数由`weakref_type`来完成。

#### 强引用计数操作

回看`RefBase`的定义，其内部持有一个`weakref_impl`的指针。

```c++
class RefBase
{
public:
    void            incStrong(const void* id) const;
    void            decStrong(const void* id) const;

    void            forceIncStrong(const void* id) const;

    //! DEBUGGING ONLY: Get current strong ref count.
    int32_t         getStrongCount() const;
private:
    weakref_impl* const mRefs;
};

RefBase::RefBase()
        : mRefs(new weakref_impl(this))
{
}
```

`RefBase`构造之初，就会完成`weakref_impl`的构造，也就是完成`weakref_impl`的4个成员变量的初始化咯。

1. 增加引用计数 -- `incStrong`

   可以猜到，就是通过原子操作`mRefs->mStrong + 1`咯。

   ```c++
   void RefBase::incStrong(const void* id) const
   {
       weakref_impl* const refs = mRefs;
       //增加弱引用计数
       refs->incWeak(id);
       //原子操作。
       const int32_t c = refs->mStrong.fetch_add(1, std::memory_order_relaxed);
       if (c != INITIAL_STRONG_VALUE)  {
           return;
       }
   
       int32_t old = refs->mStrong.fetch_sub(INITIAL_STRONG_VALUE,
                                             std::memory_order_relaxed);
       // A decStrong() must still happen after us.
       refs->mBase->onFirstRef();
   }
   ```

   嗯，在增加强引用的同时，弱引用计数也会加1，符合逻辑。并且，通过判断加1前的值是否等于`INITIAL_STRONG_VALUE`来判断是否是第一次增加强引用计数，进而回调`onFirstRef`。所以，我们在子类中，可以实现`onFirstRef`方法来完成我们需要做的一些事情（这些事情我是不是也可以放到被引用对象的构造函数中完成呢？为什么Android需要多此一举？还是另有深意？？不懂。）。
   
2. 减少强引用计数 -- `decStrong`

   `decStrong`应该会比`incStrong`复杂点，毕竟其可能需要析构对象。

   ```C++
   
   void RefBase::decStrong(const void* id) const
   {
       weakref_impl* const refs = mRefs;
       //减少计数
       const int32_t c = refs->mStrong.fetch_sub(1, std::memory_order_release);
       if (c == 1) {
           std::atomic_thread_fence(std::memory_order_acquire);
           refs->mBase->onLastStrongRef(id);
           int32_t flags = refs->mFlags.load(std::memory_order_relaxed);
           if ((flags&OBJECT_LIFETIME_MASK) == OBJECT_LIFETIME_STRONG) {
               delete this;
               // The destructor does not delete refs in this case.
           }
       }
       
       refs->decWeak(id);
   }
   
   ```
   
   当强引用计数为0时，调用`onLastStrongRef`回调。值得注意的时，这里，我们第一次看到了`refs->mFlags`的使用，so，介绍一下：
   
   ```c++
   enum {
       //强引用==0， 析构RefBase
    OBJECT_LIFETIME_STRONG  = 0x0000,
       //弱引用==0，析构RefBase
    OBJECT_LIFETIME_WEAK    = 0x0001,
    OBJECT_LIFETIME_MASK    = 0x0001
   };
   ```
   
   作用很简单，就是控制RefBase的生命周期。
   
   减少强引用计数以后，还会继续较少弱引用计数。再来看看`RefBase`的析构函数。
   
   ```c++
   RefBase::~RefBase()
   {
       int32_t flags = mRefs->mFlags.load(std::memory_order_relaxed);
       if ((flags & OBJECT_LIFETIME_MASK) == OBJECT_LIFETIME_WEAK) {
           if (mRefs->mWeak.load(std::memory_order_relaxed) == 0) {
               delete mRefs;
           }
       } else if (mRefs->mStrong.load(std::memory_order_relaxed)
                  == INITIAL_STRONG_VALUE) {
           delete mRefs;
       }
       //[1]
       const_cast<weakref_impl*&>(mRefs) = NULL;
   }
   ```




   `[1]`处代码将`RefBase::mRefs`置为`NULL`，如果此时，还存在有弱引用计数呢？这个就要看`wp<T>`的实现了。

   析构函数，根据`flag`来判断是否需要`delete mRefs`。

   ## sp 和 wp

常用方式：

```c++
class A : public RefBase{};
sp<A> spa = new A();
wp<A> wpa(spa);
```



### sp

主要看构造函数和析构函数的实现。

```c++
template<typename T>
sp<T>::sp(T* other)
        : m_ptr(other)
{
    if (other) other->incStrong(this);
}
template<typename T>
sp<T>::~sp()
{
    if (m_ptr) m_ptr->decStrong(this);
}
```

调用的就是`RefBase->incStrong`和`RefBase->decStrong`。so easy（前提是了解 RefBase）。



### wp

```c++
template<typename T>
wp<T>::wp(T* other)
        : m_ptr(other)
{
    if (other) m_refs = other->createWeak(this);
}

template<typename T>
wp<T>::~wp()
{
    if (m_ptr) m_refs->decWeak(this);
}
```

大同小异，最终也是调用的`RefBase->mrefs->decWeak`和`RefBase->mrefs->incWeak`。



哇，似乎很简单哦，实际上，主要工作都是基类`RefBase`做了。智能指针工作量就是实现各种构造函数。

但是`wp`有一个例外，就是，要考虑：其引用的实际对象以及上西天了。。。。

so，我们在使用`wp`引用的对象时，都需要调用`promote`来将其转换成`sp`。

```c++
template<typename T>
sp<T> wp<T>::promote() const
{
    sp<T> result;
    if (m_ptr && m_refs->attemptIncStrong(&result)) {
        result.set_pointer(m_ptr);
    }
    return result;
}
```

麻蛋，这个工作还是由`RefBase`完成。see see。

```c++

bool RefBase::weakref_type::attemptIncStrong(const void* id)
{
    incWeak(id);

    weakref_impl* const impl = static_cast<weakref_impl*>(this);
    int32_t curCount = impl->mStrong.load(std::memory_order_relaxed);

    //[1] 实际对象存在
    while (curCount > 0 && curCount != INITIAL_STRONG_VALUE) {
        if (impl->mStrong.compare_exchange_weak(curCount, curCount+1,
                                                std::memory_order_relaxed)) {
            break;
        }
    }

    if (curCount <= 0 || curCount == INITIAL_STRONG_VALUE) {
        int32_t flags = impl->mFlags.load(std::memory_order_relaxed);
        if ((flags&OBJECT_LIFETIME_MASK) == OBJECT_LIFETIME_STRONG) {
            if (curCount <= 0) {
                decWeak(id);
                return false;
            }

            while (curCount > 0) {
                //这里会完成强引用计数的+1.
               	//如果，到这里，其引用对象析构，curCount < 0
                if (impl->mStrong.compare_exchange_weak(curCount, curCount+1,
                                                        std::memory_order_relaxed)) {
                    break;
                }
            }

            if (curCount <= 0) {
                decWeak(id);
                return false;
            }
        } else {
            if (!impl->mBase->onIncStrongAttempted(FIRST_INC_STRONG, id)) {
                decWeak(id);
                return false;
            }
            curCount = impl->mStrong.fetch_add(1, std::memory_order_relaxed);
            if (curCount != 0 && curCount != INITIAL_STRONG_VALUE) {
                impl->mBase->onLastStrongRef(id);
            }
        }
    }

    impl->addStrongRef(id);
    if (curCount == INITIAL_STRONG_VALUE) {
        impl->mStrong.fetch_sub(INITIAL_STRONG_VALUE,
                                std::memory_order_relaxed);
    }

    return true;
}
```



总结一下实现：

1. 实际对象存在，那就直接提升为sp咯。

2. 实际对象是`INIT`状态。

   ```c++
   //like this
   wp<A> wpa(new A());
   ```

   像这种没有经过`spa`构造过的，先增加强引用计数，再减去`INITIAL_STRONG_VALUE`。

3. 最难处理的情况。涉及到C++标准库的atomic以及 RefBase的生命周期。不解释，不想看，看不懂，OK？？





## 总结

学习这里的目的主要是为了更加流畅的阅读Framework的native代码。so，点到为止，有时间再去学习那些技术。