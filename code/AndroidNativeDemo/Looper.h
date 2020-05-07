//
// Created by lz153 on 2020/5/7.
//

#ifndef DEMO_LOOPER_H
#define DEMO_LOOPER_H

#include "RefBase.h"
#include <mutex>


typedef int (*Looper_callbackFunc)(int fd, int events, void* data);

struct Message {
    Message() : what(0) { }
    Message(int w) : what(w) { }

    //消息类型
    int what;
};

class MessageHandler : public virtual RefBase {
protected:
    virtual ~MessageHandler() { };

public:
    virtual void handleMessage(const Message &message) = 0;
};

class WeakMessageHandler : public MessageHandler {
protected:
    virtual ~WeakMessageHandler();

public:
    WeakMessageHandler(const sp<MessageHandler> &handler);

    virtual void handleMessage(const Message& message);

private:
    wp<MessageHandler> mHandler;
};

class LooperCallback : public virtual RefBase {
protected:
    virtual ~LooperCallback() {};

public:
    virtual int handleEvent(int fd, int events, void *data) = 0;
};


class SimpleLooperCallback : public LooperCallback {
protected:
    virtual ~SimpleLooperCallback();

public:
    SimpleLooperCallback(Looper_callbackFunc callback);
    virtual int handleEvent(int fd, int events, void *data);

private:
    Looper_callbackFunc mCallback;
};


class Looper : public RefBase {
private:
    virtual ~Looper() ;

public:

    enum {
        /**
         * Option for Looper_prepare: this looper will accept calls to
         * Looper_addFd() that do not have a callback (that is provide NULL
         * for the callback).  In this case the caller of Looper_pollOnce()
         * or Looper_pollAll() MUST check the return from these functions to
         * discover when data is available on such fds and process it.
         */
        PREPARE_ALLOW_NON_CALLBACKS = 1 << 0
    };

    // If allowNonCallbaks is true, the looper will allow file descriptors
    // to be registered without associated callbacks.
    Looper(bool allowNonCallbacks);


    bool getAllowNonCallbacks() const;


    /**
     * 异步唤醒epoll_wait,通过 eventfd
     */
    void wake();


    static sp<Looper> prepare(int opts);

    static void setForThread(const sp<Looper> &looper);

    static sp<Looper> getForThread();
private:

    struct Request {
        int fd;
        int ident;
        int events;
        int seq;
        sp<LooperCallback> callback;
        void *data;

        void initEventItem(struct epoll_event *eventItem) const;
    };

    struct Response {
        int events;
        Request request;
    };


    const bool mAllowNonCallbacks;


    int mWakeEventFd;
    std::mutex  mLock;

    int mEpollFd;
    bool mEpollRebuildRequired;


    void awoken();

    void rebuildEpollLocked();

    static void initTLSKey();
    static void threadDestructor(void *st);
    static void initEpollEvent(struct epoll_event *eventItem);
};


#endif //DEMO_LOOPER_H
