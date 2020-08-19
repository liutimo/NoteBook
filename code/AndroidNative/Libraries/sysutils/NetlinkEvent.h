//
// Created by lz153 on 2020/6/7.
//

#ifndef ANDROIDNATIVE_NETLINKEVENT_H
#define ANDROIDNATIVE_NETLINKEVENT_H


#include "NetlinkListener.h"

#define NL_PARAMS_MAX 32

class NetlinkEvent {
public:
    enum class Action {
        kUnknown = 0,
        kAdd = 1,
        kRemove = 2,
        kChange = 3,
        kLinkUp = 4,
        kLinkDown = 5,
        kAddressUpdated = 6,
        kAddressRemoved = 7,
        kRdnss = 8,
        kRouteUpdated = 9,
        kRouteRemoved = 10,
    };

private:
    int  mSeq;
    char *mPath;
    Action mAction;
    char *mSubsystem;
    char *mParams[NL_PARAMS_MAX];

public:
    NetlinkEvent();
    virtual ~NetlinkEvent();

    bool decode(char *buffer, int size, int format = NetlinkListener::NETLINK_FORMAT_ASCII);
    const char *findParam(const char *paramName);

    const char *getSubsystem() { return mSubsystem; }
    Action getAction() { return mAction; }

    void dump();

protected:
    bool parseBinaryNetlinkMessage(char *buffer, int size);
    bool parseAsciiNetlinkMessage(char *buffer, int size);
    bool parseIfInfoMessage(const struct nlmsghdr *nh);
    bool parseIfAddrMessage(const struct nlmsghdr *nh);
    bool parseUlogPacketMessage(const struct nlmsghdr *nh);
    bool parseNfPacketMessage(struct nlmsghdr *nh);
    bool parseRtMessage(const struct nlmsghdr *nh);
    bool parseNdUserOptMessage(const struct nlmsghdr *nh);
};


#endif //ANDROIDNATIVE_NETLINKEVENT_H
