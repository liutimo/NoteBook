## main函数

```c++
int main(int argc __unused, char **argv __unused)
{
    InitPlayer();
    signal(SIGPIPE, SIG_IGN);

    sp<ProcessState> proc(ProcessState::self());     //[1]
    sp<IServiceManager> sm(defaultServiceManager()); //[2]
    ALOGI("ServiceManager: %p", sm.get());
    InitializeIcuOrDie();			//[3]
    MediaPlayerService::instantiate();  //[4]
    ResourceManagerService::instantiate();  //[5]
    registerExtensions();   //[6]
    ProcessState::self()->startThreadPool();  //[7]
    IPCThreadState::self()->joinThreadPool();  //[8]
}
```

1. ProcessState，单例模式。负责打开Binder驱动并完成内存映射

2. `defaultServiceManager`

    其实现可以精简为:

    ```c++
    interface_cast<IServiceManager>(ProcessState::self()->getContextObject(NULL));
    ```

    又回到了`ProcessState`。

    ```c++
    sp<IBinder> ProcessState::getContextObject(const sp<IBinder>& /*caller*/)
    {
        return getStrongProxyForHandle(0);
    }
    
    sp<IBinder> ProcessState::getStrongProxyForHandle(int32_t handle)
    {
        sp<IBinder> result;
    
        AutoMutex _l(mLock);
    
        //handle == 0， 该处相当于创建一个新的handle_entry
        //其binder 和 refs 成员都为NULL。
        handle_entry* e = lookupHandleLocked(handle);
    
        if (e != NULL) {
            IBinder* b = e->binder;		//b == NULL
            if (b == NULL || !e->refs->attemptIncWeak(this)) {
                if (handle == 0) {
                    Parcel data;
                    status_t status = IPCThreadState::self()->transact(
                            0, IBinder::PING_TRANSACTION, data, NULL, 0);
                    if (status == DEAD_OBJECT)
                       return NULL;
                }
    
                b = new BpBinder(handle); 
                e->binder = b;
                if (b) e->refs = b->getWeakRefs();
                result = b;
            } else {
                // This little bit of nastyness is to allow us to add a primary
                // reference to the remote proxy when this team doesn't have one
                // but another team is sending the handle to us.
                result.force_set(b);
                e->refs->decWeak(this);
            }
        }
        return result;
    }
    ```

    由代码可知。`defaultServiceManager`最终将一个`BpBinder`转换成`IServiceManager`(通过`interface_cast`)

    ```c++
    template<typename INTERFACE>
    inline sp<INTERFACE> interface_cast(const sp<IBinder>& obj)
    {
        return INTERFACE::asInterface(obj);
    }
    
     DECLARE_META_INTERFACE(ServiceManager);
    
                               
    static const android::String16 descriptor;                         
    static android::sp<IServiceManager> asInterface(                     
                const android::sp<android::IBinder>& obj);                 
    virtual const android::String16& getInterfaceDescriptor() const;   
    IServiceManager();                                                     
    virtual ~IServiceManager();                                            
        
        
    DECLARE_META_INTERFACE(ServiceManager);
    
    #define IMPLEMENT_META_INTERFACE(INTERFACE, NAME)                       
        const android::String16 IServiceManager::descriptor("android.os.IServiceManager");             
        const android::String16&                                            
                IServiceManager::getInterfaceDescriptor() const {              
            return IServiceManager::descriptor;                                
        }                                                                   
        android::sp<IServiceManager> IServiceManager::asInterface(                
                const android::sp<android::IBinder>& obj)                   
        {                                                                   
            android::sp<IServiceManager> intr;                                 
            if (obj != NULL) {                                              
                intr = static_cast<IServiceManager*>(                          
                    obj->queryLocalInterface(                               
                            IServiceManager::descriptor).get());               
                if (intr == NULL) {                                         
                    intr = new BpServiceManager(obj);                          
                }                                                           
            }                                                               
            return intr;                                                   
        }                                           
    
    IMPLEMENT_META_INTERFACE(ServiceManager, "android.os.IServiceManager");
    ```
    
    