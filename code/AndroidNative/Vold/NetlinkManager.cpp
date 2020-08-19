//
// Created by lz153 on 2020/6/6.
//

#include "NetlinkManager.h"
#include "NetlinkHandler.h"
#include <sys/socket.h>
#include <linux/netlink.h>
#include <cstring>
#include <unistd.h>

NetlinkManager *NetlinkManager::instance() {
    if (!sInstace) {
        sInstace = new NetlinkManager;
    }

    return sInstace;
}

NetlinkManager::NetlinkManager() {
    mBroadcaster = nullptr;
}

NetlinkManager::~NetlinkManager() {

}

int NetlinkManager::start() {
    struct sockaddr_nl nladdr;

    memset(&nladdr, 0, sizeof(nladdr));
    nladdr.nl_family = AF_NETLINK;
    nladdr.nl_groups = 0xFFFFFFFF;
    nladdr.nl_pid = getpid();

    if ((mSock = socket(PF_NETLINK, SOCK_DGRAM | SOCK_CLOEXEC, NETLINK_KOBJECT_UEVENT)) < 0) {
        return -1;
    }

    int sz = 64 * 1024;
    if (setsockopt(mSock, SOL_SOCKET, SO_RCVBUFFORCE, &sz, sizeof(sz)) < 0) {
        goto out;
    }

    int on = 1;
    if (setsockopt(mSock, SOL_SOCKET, SO_PASSCRED, &on, sizeof(on)) < 0) {
        goto out;
    }

    if (bind(mSock, (struct sockaddr*)&nladdr, sizeof(nladdr)) < 0) {
        goto out;
    }

    mHandler = new NetlinkHandler(mSock);
    if (mHandler->start()) {
        goto out;
    }
    return 0;
out:
    close(mSock);
    return -1;
}


int NetlinkManager::stop() {
    int status = 0;

    if (mHandler->stop()) {
        status = -1;
    }

    delete mHandler;
    mHandler = nullptr;
    close(mSock);
    mSock = -1;
    return status;
}

