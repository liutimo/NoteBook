//
// Created by lz153 on 2020/6/7.
//

#include <cstring>
#include <cstdlib>
#include "NetlinkEvent.h"
#include <linux/if_addr.h>
#include <linux/if_link.h>
#include <linux/rtnetlink.h>
#include <linux/netfilter/nfnetlink.h>
#include <linux/netfilter/nfnetlink_log.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <linux/if.h>

const int LOCAL_QLOG_NL_EVENT = 112;
const int LOCAL_NFLOG_PACKET = NFNL_SUBSYS_ULOG << 8 | NFULNL_MSG_PACKET;


NetlinkEvent::NetlinkEvent() {
    mAction = Action::kUnknown;
    memset(mParams, 0, sizeof(mParams));
    mPath = nullptr;
    mSubsystem = nullptr;
}

NetlinkEvent::~NetlinkEvent() {
    int i = 0;
    if (mPath) {
        free(mPath);
    }

    if (mSubsystem) {
        free(mSubsystem);
    }

    for (i = 0; i < NL_PARAMS_MAX; i++) {
        if (!mParams[i]) {
            break;
        }
        free(mParams[i]);
    }
}

void NetlinkEvent::dump() {

}


static const char* rtMessageName(int type) {
#define NL_EVENT_RTM_NAME(rtm) case rtm: return #rtm;
    switch (type) {
        NL_EVENT_RTM_NAME(RTM_NEWLINK);
        NL_EVENT_RTM_NAME(RTM_DELLINK);
        NL_EVENT_RTM_NAME(RTM_NEWADDR);
        NL_EVENT_RTM_NAME(RTM_DELADDR);
        NL_EVENT_RTM_NAME(RTM_NEWROUTE);
        NL_EVENT_RTM_NAME(RTM_DELROUTE);
        NL_EVENT_RTM_NAME(RTM_NEWNDUSEROPT);
        NL_EVENT_RTM_NAME(LOCAL_QLOG_NL_EVENT);
        NL_EVENT_RTM_NAME(LOCAL_NFLOG_PACKET);
        default:
            return NULL;
    }
#undef NL_EVENT_RTM_NAME
}

static bool checkRtNetlinkLength(const struct nlmsghdr* nh, size_t size) {
    return nh->nlmsg_len >= NLMSG_LENGTH(size);
}

/*
 * Parse a RTM_NEWLINK message.
 * 创建，删除或者获取网络设备的信息；
 */
bool NetlinkEvent::parseIfInfoMessage(const struct nlmsghdr *nh) {
    struct ifinfomsg *ifi = (struct ifinfomsg*)NLMSG_DATA(nh);
    if (!checkRtNetlinkLength(nh, sizeof(*ifi))) {
        return false;
    }

    if ((ifi->ifi_flags & IFF_LOOPBACK) != 0) {
        return false;
    }

    int len = IFLA_PAYLOAD(nh);
    struct rtattr *rta;
    for (rta = IFLA_RTA(ifi); RTA_OK(rta, len); rta = RTA_NEXT(rta, len)) {
        switch (rta->rta_type) {
            case IFLA_IFNAME:
                asprintf(&mParams[0], "INTERFACE=%s", (char *) RTA_DATA(rta));
                mAction = (ifi->ifi_flags & IFF_LOWER_UP) ? Action::kLinkUp :
                          Action::kLinkDown;
                mSubsystem = strdup("net");
                return true;
        }
    }
    return false;
}


/*
 * Parse a RTM_NEWADDR or RTM_DELADDR message.
 */
bool NetlinkEvent::parseIfAddrMessage(const struct nlmsghdr *nh) {
    struct ifaddrmsg *ifaddr = (struct ifaddrmsg *) NLMSG_DATA(nh);
    struct ifa_cacheinfo *cacheinfo = NULL;
    char addrstr[INET6_ADDRSTRLEN] = "";
    char ifname[IFNAMSIZ] = "";

    if (!checkRtNetlinkLength(nh, sizeof(*ifaddr)))
        return false;

    // Sanity check.
    int type = nh->nlmsg_type;
    if (type != RTM_NEWADDR && type != RTM_DELADDR) {
        return false;
    }

    // For log messages.
    const char *msgtype = rtMessageName(type);

    struct rtattr *rta;
    int len = IFA_PAYLOAD(nh);
    for (rta = IFA_RTA(ifaddr); RTA_OK(rta, len); rta = RTA_NEXT(rta, len)) {
        if (rta->rta_type == IFA_ADDRESS) {
            // Only look at the first address, because we only support notifying
            // one change at a time.
            if (*addrstr != '\0')
                continue;

            // Convert the IP address to a string.
            if (ifaddr->ifa_family == AF_INET) {
                struct in_addr *addr4 = (struct in_addr *) RTA_DATA(rta);
                if (RTA_PAYLOAD(rta) < sizeof(*addr4)) {
                    continue;
                }
                inet_ntop(AF_INET, addr4, addrstr, sizeof(addrstr));
            } else if (ifaddr->ifa_family == AF_INET6) {
                struct in6_addr *addr6 = (struct in6_addr *) RTA_DATA(rta);
                if (RTA_PAYLOAD(rta) < sizeof(*addr6)) {
                    continue;
                }
                inet_ntop(AF_INET6, addr6, addrstr, sizeof(addrstr));
            } else {
                continue;
            }
            // Find the
        } else if (rta->rta_type == IFA_CACHEINFO) {
            // Address lifetime information.
            if (cacheinfo)
                continue;

            if (RTA_PAYLOAD(rta) < sizeof(*cacheinfo)) {
                continue;
            }

            cacheinfo = (struct ifa_cacheinfo *) RTA_DATA(rta);
        }
    }

    if (addrstr[0] == '\0') {
        return false;
    }

    // Fill in netlink event information.
    mAction = (type == RTM_NEWADDR) ? Action::kAddressUpdated :
              Action::kAddressRemoved;
    mSubsystem = strdup("net");
    asprintf(&mParams[0], "ADDRESS=%s/%d", addrstr, ifaddr->ifa_prefixlen);
    asprintf(&mParams[1], "INTERFACE=%s", ifname);
    asprintf(&mParams[2], "FLAGS=%u", ifaddr->ifa_flags);
    asprintf(&mParams[3], "SCOPE=%u", ifaddr->ifa_scope);

    if (cacheinfo) {
        asprintf(&mParams[4], "PREFERRED=%u", cacheinfo->ifa_prefered);
        asprintf(&mParams[5], "VALID=%u", cacheinfo->ifa_valid);
        asprintf(&mParams[6], "CSTAMP=%u", cacheinfo->cstamp);
        asprintf(&mParams[7], "TSTAMP=%u", cacheinfo->tstamp);
    }

    return true;
}

/*
 * Parse a QLOG_NL_EVENT message.
 */
bool NetlinkEvent::parseUlogPacketMessage(const struct nlmsghdr *nh) {
//    const char *devname;
//    ulog_packet_msg_t *pm = (ulog_packet_msg_t *) NLMSG_DATA(nh);
//    if (!checkRtNetlinkLength(nh, sizeof(*pm)))
//        return false;
//
//    devname = pm->indev_name[0] ? pm->indev_name : pm->outdev_name;
//    asprintf(&mParams[0], "ALERT_NAME=%s", pm->prefix);
//    asprintf(&mParams[1], "INTERFACE=%s", devname);
//    mSubsystem = strdup("qlog");
//    mAction = Action::kChange;
    return true;
}

/*
 * Parse a LOCAL_NFLOG_PACKET message.
 */
bool NetlinkEvent::parseNfPacketMessage(struct nlmsghdr *nh) {
    return true;
}

/*
 * Parse a RTM_NEWROUTE or RTM_DELROUTE message.
 */
bool NetlinkEvent::parseRtMessage(const struct nlmsghdr *nh) {

    return true;
}

/*
 * Parse a RTM_NEWNDUSEROPT message.
 */
bool NetlinkEvent::parseNdUserOptMessage(const struct nlmsghdr *nh) {
    return true;
}

/*
 * Parse a binary message from a NETLINK_ROUTE netlink socket.
 *
 * Note that this function can only parse one message, because the message's
 * content has to be stored in the class's member variables (mAction,
 * mSubsystem, etc.). Invalid or unrecognized messages are skipped, but if
 * there are multiple valid messages in the buffer, only the first one will be
 * returned.
 *
 * TODO: consider only ever looking at the first message.
 */
bool NetlinkEvent::parseBinaryNetlinkMessage(char *buffer, int size) {
    struct nlmsghdr *nh;

    for (nh = (struct nlmsghdr *) buffer;
         NLMSG_OK(nh, (unsigned) size) && (nh->nlmsg_type != NLMSG_DONE);
         nh = NLMSG_NEXT(nh, size)) {

        if (!rtMessageName(nh->nlmsg_type)) {
            continue;
        }

        if (nh->nlmsg_type == RTM_NEWLINK) {
            if (parseIfInfoMessage(nh))
                return true;

        } else if (nh->nlmsg_type == LOCAL_QLOG_NL_EVENT) {
            if (parseUlogPacketMessage(nh))
                return true;

        } else if (nh->nlmsg_type == RTM_NEWADDR ||
                   nh->nlmsg_type == RTM_DELADDR) {
            if (parseIfAddrMessage(nh))
                return true;

        } else if (nh->nlmsg_type == RTM_NEWROUTE ||
                   nh->nlmsg_type == RTM_DELROUTE) {
            if (parseRtMessage(nh))
                return true;

        } else if (nh->nlmsg_type == RTM_NEWNDUSEROPT) {
            if (parseNdUserOptMessage(nh))
                return true;

        } else if (nh->nlmsg_type == LOCAL_NFLOG_PACKET) {
            if (parseNfPacketMessage(nh))
                return true;

        }
    }

    return false;
}

/* If the string between 'str' and 'end' begins with 'prefixlen' characters
 * from the 'prefix' array, then return 'str + prefixlen', otherwise return
 * NULL.
 */
static const char*
has_prefix(const char* str, const char* end, const char* prefix, size_t prefixlen)
{
    if ((end - str) >= (std::ptrdiff_t)prefixlen &&
        (prefixlen == 0 || !memcmp(str, prefix, prefixlen))) {
        return str + prefixlen;
    } else {
        return NULL;
    }
}

/* Same as strlen(x) for constant string literals ONLY */
#define CONST_STRLEN(x)  (sizeof(x)-1)

/* Convenience macro to call has_prefix with a constant string literal  */
#define HAS_CONST_PREFIX(str,end,prefix)  has_prefix((str),(end),prefix,CONST_STRLEN(prefix))


/*
 * Parse an ASCII-formatted message from a NETLINK_KOBJECT_UEVENT
 * netlink socket.
 */
bool NetlinkEvent::parseAsciiNetlinkMessage(char *buffer, int size) {
    const char *s = buffer;
    const char *end;
    int param_idx = 0;
    int first = 1;

    if (size == 0)
        return false;

    /* Ensure the buffer is zero-terminated, the code below depends on this */
    buffer[size-1] = '\0';

    end = s + size;
    while (s < end) {
        if (first) {
            const char *p;
            /* buffer is 0-terminated, no need to check p < end */
            for (p = s; *p != '@'; p++) {
                if (!*p) { /* no '@', should not happen */
                    return false;
                }
            }
            mPath = strdup(p+1);
            first = 0;
        } else {
            const char* a;
            if ((a = HAS_CONST_PREFIX(s, end, "ACTION=")) != NULL) {
                if (!strcmp(a, "add"))
                    mAction = Action::kAdd;
                else if (!strcmp(a, "remove"))
                    mAction = Action::kRemove;
                else if (!strcmp(a, "change"))
                    mAction = Action::kChange;
            } else if ((a = HAS_CONST_PREFIX(s, end, "SEQNUM=")) != NULL) {
                mSeq = atoi(a);
            } else if ((a = HAS_CONST_PREFIX(s, end, "SUBSYSTEM=")) != NULL) {
                mSubsystem = strdup(a);
            } else if (param_idx < NL_PARAMS_MAX) {
                mParams[param_idx++] = strdup(s);
            }
        }
        s += strlen(s) + 1;
    }
    return true;
}

bool NetlinkEvent::decode(char *buffer, int size, int format) {
    if (format == NetlinkListener::NETLINK_FORMAT_BINARY
        || format == NetlinkListener::NETLINK_FORMAT_BINARY_UNICAST) {
        return parseBinaryNetlinkMessage(buffer, size);
    } else {
        return parseAsciiNetlinkMessage(buffer, size);
    }
}

const char *NetlinkEvent::findParam(const char *paramName) {
    size_t len = strlen(paramName);
    for (int i = 0; i < NL_PARAMS_MAX && mParams[i] != NULL; ++i) {
        const char *ptr = mParams[i] + len;
        if (!strncmp(mParams[i], paramName, len) && *ptr == '=')
            return ++ptr;
    }
    return nullptr;
}
