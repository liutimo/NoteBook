//
// Created by lz153 on 2020/6/7.
//

#ifndef ANDROIDNATIVE_NETLINKHANDLER_H
#define ANDROIDNATIVE_NETLINKHANDLER_H

#include "sysutils/NetlinkListener.h"

class NetlinkHandler  : public NetlinkListener{
public:
    explicit NetlinkHandler(int listenerSocket);
    virtual ~NetlinkHandler();

    int start(void);
    int stop(void);

protected:
    void onEvent(NetlinkEvent *event) override;
};


#endif //ANDROIDNATIVE_NETLINKHANDLER_H
