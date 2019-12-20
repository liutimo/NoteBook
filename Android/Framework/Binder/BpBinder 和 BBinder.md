 `BBinder`更类似于C/S中的 server， `BpBinder`更类似于Client。

怎么理解呢？

1. `BBinder`比`BpBinder`多了一个名为`onTransact`的函数，用于处理`BpBinder`的请求。
2. `BBinder`中有一个`mExtras`成员，保存着所有与之通信的`BpBinder`实例。







## BpBinder

父类是IBinder，代表binder通信中的客户端。



```c++

class BpBinder : public IBinder
{
public:
                        BpBinder(int32_t handle);

    virtual bool        isBinderAlive() const;
    virtual status_t    pingBinder();
    virtual status_t    dump(int fd, const Vector<String16>& args);

    virtual status_t    transact(   uint32_t code,
                                    const Parcel& data,
                                    Parcel* reply,
                                    uint32_t flags = 0);

    virtual status_t    linkToDeath(const sp<DeathRecipient>& recipient,
                                    void* cookie = NULL,
                                    uint32_t flags = 0);
    virtual status_t    unlinkToDeath(  const wp<DeathRecipient>& recipient,
                                        void* cookie = NULL,
                                        uint32_t flags = 0,
                                        wp<DeathRecipient>* outRecipient = NULL);

    virtual void        attachObject(   const void* objectID,
                                        void* object,
                                        void* cleanupCookie,
                                        object_cleanup_func func);
    virtual void*       findObject(const void* objectID) const;
    virtual void        detachObject(const void* objectID);

    virtual BpBinder*   remoteBinder();

            status_t    setConstantData(const void* data, size_t size);
            void        sendObituary();

 protected:
     virtual             ~BpBinder();
     virtual void        onFirstRef();
     virtual void        onLastStrongRef(const void* id);
     virtual bool        onIncStrongAttempted(uint32_t flags, const void* id);

 private:
     const   int32_t             mHandle;

     struct Obituary {
         wp<DeathRecipient> recipient;
         void* cookie;
         uint32_t flags;
     };

             void                reportOneDeath(const Obituary& obit);
             bool                isDescriptorCached() const;

     mutable Mutex               mLock;
             volatile int32_t    mAlive;
             volatile int32_t    mObitsSent;
             Vector<Obituary>*   mObituaries;
             ObjectManager       mObjects;
             Parcel*             mConstantData;
```







# BBinder

父类是IBinder，代表binder通信中的服务端。

