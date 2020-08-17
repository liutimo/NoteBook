//
// Created by lz153 on 2020/6/7.
//

#ifndef ANDROIDNATIVE_NETLINKLISTENER_H
#define ANDROIDNATIVE_NETLINKLISTENER_H

#include "SocketListener.h"

class NetlinkEvent;

class NetlinkListener : public SocketListener {
    char mBuffer[64 * 1024] __attribute__((aligned(4)));
    int mFormat;

public:
    static const int NETLINK_FORMAT_ASCII = 0;
    static const int NETLINK_FORMAT_BINARY = 1;
    static const int NETLINK_FORMAT_BINARY_UNICAST = 2;

    NetlinkListener(int socket);
    NetlinkListener(int socket, int format);

    virtual ~NetlinkListener() {}

protected:
    bool onDataAvailable(SocketClient *c) override;
    virtual void onEvent(NetlinkEvent* event) = 0;
};


#endif //ANDROIDNATIVE_NETLINKLISTENER_H
