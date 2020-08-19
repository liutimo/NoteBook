//
// Created by lz153 on 2020/6/6.
//

#ifndef ANDROIDNATIVE_SOCKETLISTENER_H
#define ANDROIDNATIVE_SOCKETLISTENER_H

#include "SocketClient.h"
#include "SocketClientCommand.h"
#include <thread>

class SocketListener {
private:
    bool                    mListen;
    const char*             mSocketName;
    int                     mSock;
    SocketClientCollection* mClients;
    std::mutex              mClientsLock;
    int                     mCtrlPipe[2];
    std::thread*            mThread;
    bool                    mUseCmdNum;



public:
    SocketListener(const char* socketName, bool listen);
    SocketListener(const char* SocketName, bool listen, bool useCmdNum);
    SocketListener(int socketFd, bool listen);

    virtual ~SocketListener();
    int startListener();
    int startListener(int backlog);
    int stopListener();

    void sendBroadcast(int code, const char* msg, bool addErrno);

    void runOnEachSocket(SocketClientCommand* command);

    bool release(SocketClient* c);

protected:
    virtual bool onDataAvailable(SocketClient *c) = 0;

private:
    bool release(SocketClient* c, bool wakeup);
    static void* threadStart(void* obj);
    void runListener();
    void init(const char* socketName, int socketFd, bool listen, bool useCmdNum);
};


#endif //ANDROIDNATIVE_SOCKETLISTENER_H
