//
// Created by lz153 on 2020/6/6.
//

#ifndef ANDROIDNATIVE_NETLINKMANAGER_H
#define ANDROIDNATIVE_NETLINKMANAGER_H


class NetlinkManager {
private:
    static NetlinkManager* sInstace;

private:
    SocketListener *mBoradcaster;
};


#endif //ANDROIDNATIVE_NETLINKMANAGER_H
