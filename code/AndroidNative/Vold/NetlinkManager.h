//
// Created by lz153 on 2020/6/6.
//

#ifndef ANDROIDNATIVE_NETLINKMANAGER_H
#define ANDROIDNATIVE_NETLINKMANAGER_H

#include <sysutils/SocketListener.h>

class NetlinkHandler;

class NetlinkManager {
private:
    static NetlinkManager* sInstace;

private:
    SocketListener*         mBroadcaster;
    NetlinkHandler*         mHandler;
    int                     mSock;

public:
    virtual ~NetlinkManager();

    int start();
    int stop();

    void setBroadcaster(SocketListener* listener) {
         mBroadcaster = listener;
    }

    SocketListener* getBroadcaster() {
        return mBroadcaster;
    }

    static NetlinkManager* instance();

private:
    NetlinkManager();
};


#endif //ANDROIDNATIVE_NETLINKMANAGER_H
