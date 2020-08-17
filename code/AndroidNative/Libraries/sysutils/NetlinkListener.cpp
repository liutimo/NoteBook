//
// Created by lz153 on 2020/6/7.
//

#include "NetlinkListener.h"

#include "NetlinkEvent.h"
#include <unistd.h>

NetlinkListener::NetlinkListener(int socket) : SocketListener(socket, false) {
    mFormat = NETLINK_FORMAT_ASCII;
}

NetlinkListener::NetlinkListener(int socket, int format) :
                        SocketListener(socket, false), mFormat(format) {

}

static ssize_t uevent_kernel_recv(int socket, void *buffer, size_t length, bool require_group, uid_t *uid);

bool NetlinkListener::onDataAvailable(SocketClient *c) {
    int socket = c->getSocket();

    bool required_group = true;
    if (mFormat == NETLINK_FORMAT_BINARY_UNICAST) {
        required_group = false;
    }

    uid_t uid = -1;
    auto count = TEMP_FAILURE_RETRY(uevent_kernel_recv(socket,
            mBuffer, sizeof(mBuffer), required_group, &uid));

    if (count < 0) {
        return false;
    }

    NetlinkEvent* event = new NetlinkEvent();
    if (event->decode(mBuffer, count, mFormat)) {
        onEvent(event);
    } else if (mFormat != NETLINK_FORMAT_BINARY) {

    }
    delete event;
    return true;
}
