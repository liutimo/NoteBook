//
// Created by lz153 on 2020/6/6.
//

#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include "SocketListener.h"


#define CtrlPipe_Shutdown 0
#define CtrlPipe_Wakeup   1

SocketListener::SocketListener(const char *socketName, bool listen) {
    init(socketName, -1, listen, false);
}

SocketListener::SocketListener(int socketFd, bool listen) {
    init(nullptr, socketFd, listen, false);
}

SocketListener::SocketListener(const char *SocketName, bool listen, bool useCmdNum) {
    init(mSocketName, -1, listen, useCmdNum);
}

void SocketListener::init(const char *socketName, int socketFd, bool listen, bool useCmdNum) {
    mListen = listen;
    mSocketName = socketName;
    mSock = socketFd;
    mUseCmdNum = useCmdNum;
    mClients = new SocketClientCollection();
}

SocketListener::~SocketListener() {

    if (mSocketName && mSock> -1) {
        close(mSock);
    }

    if (mCtrlPipe[0] != -1) {
        close(mCtrlPipe[0]);
        close(mCtrlPipe[1]);
    }

    for (auto iter = mClients->begin(); iter != mClients->end();) {
        (*iter)->decRef();
        iter = mClients->erase(iter);
    }

    delete mClients;
}


int SocketListener::startListener() {
    return startListener(4);
}

static int android_get_control_socket(const char* sockName);

int SocketListener::startListener(int backlog) {
    if (!mSocketName && mSock == -1) {
        errno = EINVAL;
        return -1;
    } else if (mSocketName) {
        if ((mSock = android_get_control_socket(mSocketName)) < 0) {
            return -1;
        }
        fcntl(mSock, F_SETFD, FD_CLOEXEC);
    }

    if (mListen && listen(mSock, backlog) < 0) {
        return -1;
    } else if (!mListen) {
        //不支持listen 的描述符，加入到mClients
        mClients->push_back(new SocketClient(mSock, false, mUseCmdNum));
    }

    if (pipe(mCtrlPipe)) {
        return -1;
    }

    mThread = new std::thread(SocketListener::threadStart, this);
    return 0;
}

int SocketListener::stopListener() {
    char c = CtrlPipe_Shutdown;

    int ret = TEMP_FAILURE_RETRY(write(mCtrlPipe[1], &c, 1));
    if (ret != 1) {
        return  -1;
    }

    mThread->join();

    close(mCtrlPipe[0]);
    close(mCtrlPipe[1]);
    mCtrlPipe[0] = -1;
    mCtrlPipe[1] = -1;

    if (mSocketName && mSock > -1) {
        close(mSock);
        mSock = -1;
    }

    for (auto iter = mClients->begin(); iter != mClients->end();) {
        delete (*iter);
        iter = mClients->erase(iter);
    }
}

void * SocketListener::threadStart(void *obj) {
    SocketListener* self = reinterpret_cast<SocketListener*>(obj);

    self->runListener();
    return nullptr;
}


void SocketListener::runListener() {
    SocketClientCollection pendingList;

    while (1) {

        fd_set read_fds;
        int rc = 0;
        int max = -1;

        FD_ZERO(&read_fds);
        if (mListen) {
            max = mSock;
            FD_SET(mSock, &read_fds);
        }

        FD_SET(mCtrlPipe[0], &read_fds);
        if (mCtrlPipe[0] > max) {
            max = mCtrlPipe[0];
        }

        mClientsLock.lock();
        for (auto iter = mClients->begin(); iter != mClients->end(); ++iter) {
            int fd = (*iter)->getSocket();
            FD_SET(fd, &read_fds);
            max = std::max(fd, max);
        }
        mClientsLock.unlock();

        /**
         * 每次select前，通过上面的代码来设置需要监听的文件描述符。
         */
        if ((rc = select(max + 1, &read_fds, nullptr, nullptr, nullptr)) < 0) {
            //处理被信号中断的场景
            if (errno = EINTR) {
                continue;
            }
            continue;
        } else if (!rc) {
            continue;
        }


        if (FD_ISSET(mCtrlPipe[0], &read_fds)) {
            char c = CtrlPipe_Shutdown;
            TEMP_FAILURE_RETRY(read(mCtrlPipe[0], &c, 1));
            if (c == CtrlPipe_Shutdown) {
                break;
            }
            continue;
        }

        if (mListen && FD_ISSET(mSock, &read_fds)) {
            int c = TEMP_FAILURE_RETRY(accept4(mSock, nullptr, nullptr, SOCK_CLOEXEC));
            if (c < 0) {
                continue;
            }

            mClientsLock.lock();
            mClients->push_back(new SocketClient(c, true, mUseCmdNum));
            mClientsLock.unlock();
        }

        pendingList.clear();
        mClientsLock.lock();

        for (auto iter = mClients->begin(); iter != mClients->end(); ++iter) {
            SocketClient* client = *iter;

            int fd = client->getSocket();
            if (FD_ISSET(fd, &read_fds)) {
                pendingList.push_back(client);
                client->incRef();
            }
        }
        mClientsLock.unlock();

        while (!pendingList.empty()) {
            auto iter = pendingList.begin();
            SocketClient* client = *iter;
            pendingList.erase(iter);

            if (!onDataAvailable(client)) {
                release(client, false);
            }
            client->decRef();
        }
    }
}


bool SocketListener::release(SocketClient *c) {
    return release(c, true);
}



bool SocketListener::release(SocketClient *c, bool wakeup) {
    bool ret = false;
    /* if our sockets are connection-based, remove and destroy it */
    if (mListen && c) {
        /* Remove the client from our array */
        mClientsLock.lock();
        SocketClientCollection::iterator it;
        for (it = mClients->begin(); it != mClients->end(); ++it) {
            if (*it == c) {
                mClients->erase(it);
                ret = true;
                break;
            }
        }
        mClientsLock.unlock();
        if (ret) {
            ret = c->decRef();
            if (wakeup) {
                char b = CtrlPipe_Wakeup;
                TEMP_FAILURE_RETRY(write(mCtrlPipe[1], &b, 1));
            }
        }
    }
    return ret;
}

void SocketListener::sendBroadcast(int code, const char *msg, bool addErrno) {
    SocketClientCollection safeList;

    /* Add all active clients to the safe list first */
    safeList.clear();
    mClientsLock.lock();
    SocketClientCollection::iterator i;

    for (i = mClients->begin(); i != mClients->end(); ++i) {
        SocketClient* c = *i;
        c->incRef();
        safeList.push_back(c);
    }
    mClientsLock.unlock();
    while (!safeList.empty()) {
        /* Pop the first item from the list */
        i = safeList.begin();
        SocketClient* c = *i;
        safeList.erase(i);
        // broadcasts are unsolicited and should not include a cmd number
        if (c->sendMsg(code, msg, addErrno, false)) {
        }
        c->decRef();
    }
}

void SocketListener::runOnEachSocket(SocketClientCommand *command) {
    SocketClientCollection safeList;

    /* Add all active clients to the safe list first */
    safeList.clear();
    mClientsLock.lock();
    SocketClientCollection::iterator i;

    for (i = mClients->begin(); i != mClients->end(); ++i) {
        SocketClient* c = *i;
        c->incRef();
        safeList.push_back(c);
    }
    mClientsLock.unlock();

    while (!safeList.empty()) {
        /* Pop the first item from the list */
        i = safeList.begin();
        SocketClient* c = *i;
        safeList.erase(i);
        command->runSocketCommand(c);
        c->decRef();
    }
}
