//
// Created by lz153 on 2020/5/7.
//

#ifndef DEMO_LOOPER_H
#define DEMO_LOOPER_H

#include "RefBase.h"
#include <mutex>
#include <map>
#include <vector>

using nsecs_t = int64_t;
typedef int (*Looper_callbackFunc)(int fd, int events, void* data);

struct Message {
    Message() : what(0) { }
    Message(int w) : what(w) { }

    //消息类型
    int what;
};

class MessageHandler : public virtual RefBase {
protected:
    virtual ~MessageHandler();

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
    virtual ~LooperCallback();

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
        EVENT_INPUT = 1 << 0,

        EVENT_OUTPUT = 1 << 1,

        EVENT_ERROR = 1 << 2,

        EVENT_HANGUP = 1 << 3,

        EVENT_INVALID = 1 << 4,
    };

    enum {
        POLL_WAKE = -1,

        POLL_CALLBACK = -2,

        POLL_TIMEOUT = -3,

        POLL_ERROR = -4,
    };
    enum {
        PREPARE_ALLOW_NON_CALLBACKS = 1 << 0
    };



    // If allowNonCallbaks is true, the looper will allow file descriptors
    // to be registered without associated callbacks.
    Looper(bool allowNonCallbacks);


    bool getAllowNonCallbacks() const;


    int pollOnce(int timeoutMillis, int* outFd, int* outEvents, void** outData);
    inline int pollOnce(int timeoutMillis) {
        return pollOnce(timeoutMillis, nullptr, nullptr, nullptr);
    }

    /**
     * 异步唤醒epoll_wait,通过 eventfd
     */
    void wake();


    /**
     * Adds a new file descriptor to be polled by the looper.
     * If the same file descriptor was previously added, it is replaced.
     * @return  Returns 1 if the file descriptor was added, 0 if the arguments were invalid.
     */
    int addFd(int fd, int ident, int events, Looper_callbackFunc callback, void* data);
    int addFd(int fd, int ident, int events, const sp<LooperCallback>& callback, void* data);

    int removeFd(int fd);

    void sendMessageAtTime(nsecs_t uptime, const sp<MessageHandler>& handler,
                           const Message& message);

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

    struct MessageEnvlope {
        MessageEnvlope() : uptime(0) { }
        MessageEnvlope(nsecs_t u, const sp<MessageHandler> h, const Message &m) :
            uptime(u), handler(h), message(m) {

        }

        nsecs_t uptime;
        sp<MessageHandler> handler;
        Message message;
    };


    const bool mAllowNonCallbacks;


    int mWakeEventFd;
    std::mutex  mLock;

    std::vector<MessageEnvlope> mMessageEnvelopes;
    bool mSendingMessage;

    volatile bool mPolling;

    int mEpollFd;
    bool mEpollRebuildRequired;

    //fd : Request
    std::vector<std::pair<int, Request>> mRequests;
    int mNextRequestSeq;


    std::vector<Response> mResponses;
    size_t mResponseIndex;
    nsecs_t mNextMessageUptime; // set to LLONG_MAX when none

    void awoken();

    int removeFd(int fd, int seq);


    int pollInner(int timeoutMillis);
    void pushResponse(int events, const Request &request);
    void rebuildEpollLocked();


    static void initTLSKey();
    static void threadDestructor(void *st);
    static void initEpollEvent(struct epoll_event *eventItem);
};


#endif //DEMO_LOOPER_H
