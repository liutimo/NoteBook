//
// Created by lz153 on 2020/5/7.
//

#include "Looper.h"
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

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
    Looper *const self = static_cast<Looper*>(st);
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

    // TODO 1
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