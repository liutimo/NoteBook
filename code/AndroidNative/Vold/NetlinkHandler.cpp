//
// Created by lz153 on 2020/6/7.
//

#include <cstring>
#include <sysutils/NetlinkEvent.h>
#include "NetlinkHandler.h"

NetlinkHandler::NetlinkHandler(int listenerSocket) : NetlinkListener(listenerSocket) {

}


NetlinkHandler::~NetlinkHandler() noexcept {

}

int NetlinkHandler::start() {
    return this->startListener();
}

int NetlinkHandler::stop() {
    return this->stopListener();
}

void NetlinkHandler::onEvent(NetlinkEvent *event) {

    VolumeManager* vm = VolumeManager::Instance();

    const char* subsys = event->getSubsystem();
    if (!subsys) {
        return;
    }

    if (!strcmp(subsys, "block")) {
        vm->handleBlockEvent(event);
    }
}