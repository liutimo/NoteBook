//
// Created by lz153 on 2020/6/7.
//

#ifndef ANDROIDNATIVE_VOLUMEMANAGER_H
#define ANDROIDNATIVE_VOLUMEMANAGER_H
#include <list>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include <sysutils/SocketListener.h>
#include <sysutils/NetlinkEvent.h>


class VolumeManager {

public:
    static const char *SEC_ASECDIR_EXT;
    static const char *SEC_ASECDIR_INT;
    static const char *ASECDIR;
    static const char *LOOPDIR;

private:
    static VolumeManager *sInstance;
    SocketListener      *mBroadcaster;

    bool

};


#endif //ANDROIDNATIVE_VOLUMEMANAGER_H
