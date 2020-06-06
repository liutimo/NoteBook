//
// Created by lz153 on 2020/6/6.
//

#ifndef ANDROIDNATIVE_SOCKETLISTENER_H
#define ANDROIDNATIVE_SOCKETLISTENER_H

#include "SocketClient.h"



class SocketListener {


private:
    bool            mListen;
    const char*     mSocketName;
    int             mSock;

};


#endif //ANDROIDNATIVE_SOCKETLISTENER_H
