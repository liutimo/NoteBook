//
// Created by lz153 on 2020/5/7.
//

#include "Looper.h"
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <cstring>
#include <climits>

using namespace std;
using namespace std::chrono;


WeakMessageHandler::WeakMessageHandler(const sp<MessageHandler> &handler) :
    mHandler(handler) {
}

WeakMessageHandler::~WeakMessageHandler() {

}

void WeakMessageHandler::handleMessage(const Message &message) {
    sp<MessageHandler> handler = mHandler.promote();
    if (handler != nullptr) {
        handler->handleMessage(message);
    }
}


// --- SimpleLooperCallback ---

SimpleLooperCallback::SimpleLooperCallback(Looper_callbackFunc callback) :
    mCallback(callback) {

}

SimpleLooperCallback::~SimpleLooperCallback() {

}

int SimpleLooperCallback::handleEvent(int fd, int events, void *data) {
    return mCallback(fd, events, data);
}

// --- Looper ---

static pthread_once_t   gTLSOnce = PTHREAD_ONCE_INIT;
static pthread_key_t    gTLSKey = 0;

Looper::Looper(bool allowNonCallbacks) :
    mAllowNonCallbacks(allowNonCallbacks),
    mEpollFd(-1), mEpollRebuildRequired(false) {

    //[1]
    mWakeEventFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);

    std::lock_guard<std::mutex> lockGuard(mLock);
    rebuildEpollLocked();
}

Looper::~Looper() noexcept {
    close(mWakeEventFd);
    mWakeEventFd = -1;
    if (mEpollFd >= 0) {
        close(mEpollFd);
    }
}

//gTLSOnce, 一次线程只执行一次
void Looper::initTLSKey() {
    pthread_key_create(&gTLSKey, threadDestructor);
}

/**
 * 为了保证Looper创建后一直有效，我们手动调用了 incStrong。
 * so，并将其设置为 TLS。
 * 为了避免内存泄漏，线程退出时，需要手动盗用 decStrong。
 * @param st
 */
void Looper::threadDestructor(void *st) {
    auto *const self = static_cast<Looper*>(st);
    if (self != nullptr) {
        self->decStrong((void*)threadDestructor);
    }
}



bool Looper::getAllowNonCallbacks() const {
    return mAllowNonCallbacks;
}

sp<Looper> Looper::getForThread() {
    pthread_once(&gTLSOnce, initTLSKey);
    return (Looper*)pthread_getspecific(gTLSKey);
}

void Looper::setForThread(const sp<Looper> &looper) {
    sp<Looper> old = getForThread();

    if (looper != nullptr) {
        //这里手动管理是因为，要保证Looper对象创建后一直存活。
        looper->incStrong((void*)threadDestructor);
    }

    pthread_setspecific(gTLSKey, looper.get());
    if (old != nullptr) {
        old->decStrong((void*)threadDestructor);
    }
}

sp<Looper> Looper::prepare(int opts) {
    bool allowNonCallbacks = opts & PREPARE_ALLOW_NON_CALLBACKS;
    sp<Looper> looper = Looper::getForThread();
    if (looper == nullptr) {
        looper = new Looper(allowNonCallbacks);
        Looper::setForThread(looper);
    }
    return looper;
}



void Looper::rebuildEpollLocked() {
    if (mEpollFd >= 0) {
        close(mEpollFd);
    }

    mEpollFd = epoll_create1(EPOLL_CLOEXEC);

    //add eventfd
    struct epoll_event eventItem{};
    eventItem.events = EPOLLIN;
    eventItem.data.fd = mWakeEventFd;

    epoll_ctl(mEpollFd, EPOLL_CTL_ADD, mWakeEventFd, &eventItem);

    /*
     * rebuild
     * 需要重新注册上一次的Request
     * 第一次创建Looper mRequests is empty
     */
    for (auto & pair : mRequests) {
        const Request & request = pair.second;

        struct epoll_event eventItem{};
        request.initEventItem(&eventItem);
        epoll_ctl(mEpollFd, EPOLL_CTL_ADD, request.fd, &eventItem);
    }
}


/**
 * 原理就是通过eventfd，唤醒epoll_wait
 */
void Looper::wake() {
    uint64_t inc = 1;
    write(mWakeEventFd, &inc, sizeof(uint64_t));
}

void Looper::awoken() {
    uint64_t counter;
    read(mWakeEventFd, &counter, sizeof(counter));
}


int Looper::addFd(int fd, int ident, int events, Looper_callbackFunc callback, void *data) {
    return addFd(fd, ident, events, callback ? new SimpleLooperCallback(callback) : nullptr, data);
}

int Looper::addFd(int fd, int ident, int events, const sp<LooperCallback> &callback, void *data) {
    if (!callback.get()) {
        if (!mAllowNonCallbacks) {
            return -1;
        }

        if (ident < 0) {
            return -1;
        }
    } else {
        ident = POLL_CALLBACK;
    }

    {
        std::lock_guard<std::mutex> lockGuard(mLock);

        Request request;
        request.fd = fd;
        request.ident = ident;
        request.events = events;
        request.seq = mNextRequestSeq++;
        request.callback = callback;
        request.data = data;

        if (mNextRequestSeq == -1) {
            mNextRequestSeq = 0;
        }

        struct epoll_event eventItem{};
        request.initEventItem(&eventItem);

        int requestIndex = -1;
        for (auto &item : mRequests) {
            if (item.first == fd){
                requestIndex = 0;

                //TODO 可能会因为旧的描述符再回调函数中被关闭导致 epoll_ctl调用失败的情况
                epoll_ctl(mEpollFd, EPOLL_CTL_MOD, fd, &eventItem);
                break;
            }
        }
        if (requestIndex == -1) {
            epoll_ctl(mEpollFd, EPOLL_CTL_ADD, fd, &eventItem);
            mRequests.emplace_back(fd, request);
        }
    }
}

int Looper::removeFd(int fd) {
    return removeFd(fd, -1);
}

int Looper::removeFd(int fd, int seq) {
    {
        std::lock_guard<std::mutex> lockGuard(mLock);
        auto iter = mRequests.begin();
        for (; iter != mRequests.end(); ++iter) {
            if (iter->first == fd) {
                break;
            }
        }

        if (iter == mRequests.end()) {
            return 0;
        }

        if (seq != -1 && iter->second.seq != seq) {
            return 0;
        }

        mRequests.erase(iter);

        //TODO 和addFd类似，这里也要考虑del前fd失效的问题，
        epoll_ctl(mEpollFd, EPOLL_CTL_DEL, fd, nullptr);
    }
    return 1;
}

void Looper::sendMessageAtTime(nsecs_t uptime, const sp<MessageHandler> &handler, const Message &message) {

    auto iter = mMessageEnvelopes.cbegin();
    {
        std::lock_guard<std::mutex> lockGuard(mLock);

        while (iter !=  mMessageEnvelopes.cend() && uptime >= iter->uptime) {
            ++iter;
        }

        MessageEnvlope messageEnvlope(uptime, handler, message);
        mMessageEnvelopes.insert(iter, 1, messageEnvlope);

        if (mSendingMessage) {
            return;
        }
    }

    //Wake the poll loop only when we enqueue a new message at the head.
    if (iter == mMessageEnvelopes.cbegin()) {
        wake();
    }
}

int Looper::pollOnce(int timeoutMillis, int *outFd, int *outEvents, void **outData) {

    int result = 0;
    for(;;) {
        while (mResponseIndex < mResponses.size()) {
            const Response& response = mResponses[mResponseIndex++];
            int ident = response.request.ident;
            if (ident >= 0) {
                int fd = response.request.fd;
                int events = response.events;
                void* data = response.request.data;
                if (outFd != nullptr)
                    *outFd = fd;
                if (outEvents != nullptr)
                    *outEvents = events;
                if (outData != nullptr)
                    *outData = data;
                return ident;
            }
        }
        if (result != 0) {
            if (outFd != nullptr)
                *outFd = 0;
            if (outEvents != nullptr)
                *outEvents = 0;
            if (outData != nullptr)
                *outData = nullptr;
            return result;
        }
        result = pollInner(timeoutMillis);
    }
}

int Looper::pollInner(int timeoutMillis) {
    if (timeoutMillis != 0 && mNextMessageUptime != LLONG_MAX) {
        nsecs_t now = duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count();
//        int messageTimeoutMillis =
    }


    int result = POLL_WAKE;
    mResponses.clear();
    mResponseIndex = 0;

    mPolling = true;

    struct epoll_event eventItems[8];
    int eventCount = epoll_wait(mEpollFd, eventItems, 8, timeoutMillis);

    mPolling = false;

    mLock.lock();

    if (mEpollRebuildRequired) {
        mEpollRebuildRequired = false;
        rebuildEpollLocked();
        goto Done;
    }

    if (eventCount < 0) {
        if (errno == EINTR) {
            goto Done;
        }
        result = POLL_ERROR;
        goto Done;
    }

    if (eventCount == 0) {
        result = POLL_TIMEOUT;
        goto Done;
    }

    for (int i = 0; i < eventCount; ++i) {
        int fd = eventItems[i].data.fd;
        uint32_t epollEvents = eventItems[i].events;
        if (fd == mWakeEventFd) {
            if (epollEvents & EPOLLIN) {
                awoken();
            }
        } else {
            auto iter = mRequests.begin();
            for (; iter != mRequests.end(); ++iter) {
                if (iter->first == fd) {
                    break;
                }
            }
            if (iter != mRequests.end()) {
                int events = 0;
                if (epollEvents & EPOLLIN) events |= EVENT_INPUT;
                if (epollEvents & EPOLLOUT) events |= EVENT_OUTPUT;
                if (epollEvents & EPOLLERR) events |= EVENT_ERROR;
                if (epollEvents & EPOLLHUP) events |= EVENT_HANGUP;

                //构造Response
                pushResponse(events, iter->second);
            }
        }
    }

Done:
    mNextMessageUptime = LLONG_MAX;
    // Invoke pending message callbacks.
    while (mMessageEnvelopes.size() != 0) {
        nsecs_t now = duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count();
        const MessageEnvlope& messageEnvlope = mMessageEnvelopes[0];

        if (messageEnvlope.uptime <= now) {
            {
                sp<MessageHandler> handler = messageEnvlope.handler;
                Message message = messageEnvlope.message;
                mMessageEnvelopes.erase(mMessageEnvelopes.begin());
                mSendingMessage = true;
                mLock.unlock();
                handler->handleMessage(message);
            }

            mLock.lock();
            mSendingMessage = false;
            result = POLL_CALLBACK;
        } else {
            break;
        }
    }

    mLock.unlock();

    //Invoke all response callbacks.
    for (size_t i = 0; i < mResponses.size(); ++i) {
        Response& response = mResponses[i];
        if (response.request.ident == POLL_CALLBACK) {
            int fd = response.request.fd;
            int events = response.events;
            void* data = response.request.data;

            int callbackResult = response.request.callback->handleEvent(fd, events, data);
            if (callbackResult == 0) {
                removeFd(fd, response.request.seq);
            }

            response.request.callback.clear();
            result = POLL_CALLBACK;
        }
    }
    return result;
}

void Looper::pushResponse(int events, const Looper::Request &request) {
    Response response;
    response.events = events;
    response.request = request;
    mResponses.emplace_back(response);
}

void Looper::Request::initEventItem(struct epoll_event *eventItem) const {
    int epollEvents = 0;
    if (events & EVENT_INPUT)
        epollEvents |= EPOLLIN;

    if (events & EVENT_OUTPUT)
        epollEvents |= EPOLLOUT;

    memset(eventItem, 0, sizeof(*eventItem));
    eventItem->events = epollEvents;
    eventItem->data.fd = fd;
}



MessageHandler::~MessageHandler() { }

LooperCallback::~LooperCallback() { }