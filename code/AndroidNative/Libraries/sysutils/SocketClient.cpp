//
// Created by lz153 on 2020/6/6.
//

#include <cstring>
#include <unistd.h>
#include <signal.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include "SocketClient.h"

int SocketClient::sendMsg(int code, const char *msg, bool addErrno) {
    return sendMsg(code, msg, addErrno, mUseCmdNum);
}


int SocketClient::sendMsg(int code, const char *msg, bool addErrno, bool useCmdNum) {

    char *buf;
    int ret = 0;

    if (addErrno) {
        if (useCmdNum) {
            ret = asprintf(&buf, "%d %d %s (%s)", code, getCmdNum(), msg, strerror(errno));
        } else {
            ret = asprintf(&buf, "%d %s (%s)", code, msg, strerror(errno));
        }
    } else {
        if (useCmdNum) {
            ret = asprintf(&buf, "%d %d %s", code, getCmdNum(), msg);
        } else {
            ret = asprintf(&buf, "%d %s", code, msg);
        }
    }
    // Send the zero-terminated message
    if (ret != -1) {
        ret = sendMsg(buf);
        free(buf);
    }

    return ret;
}

int SocketClient::sendBinaryMsg(int code, const void *data, int len) {
    char buf[8];

    snprintf(buf, 4, "%.3d", code);
    uint32_t temp = htonl(len);
    memcpy(buf + 4, &temp, sizeof(uint32_t));

    struct iovec vec[2];
    vec[0].iov_base = buf;
    vec[0].iov_len = sizeof(buf);
    vec[1].iov_base = const_cast<void *>(data);
    vec[1].iov_len = len;

    mWriteMutex.lock();
    int result = sendDataLockedv(vec, (len > 0) ? 2 : 1);
    mWriteMutex.unlock();
    return result;
}

int SocketClient::sendCode(int code) {
    char buf[4];
    snprintf(buf, sizeof(buf), "%.3d", code);
    return sendData(buf, sizeof(buf));
}

char *SocketClient::quoteArg(const char *arg) {
    int len = strlen(arg);
    char *result = (char *) malloc(len * 2 + 3);
    char *current = result;
    const char *end = arg + len;
    char *oldresult;

    if (result == NULL) {
        return NULL;
    }

    *(current++) = '"';
    while (arg < end) {
        switch (*arg) {
            case '\\':
            case '"':
                *(current++) = '\\'; // fallthrough
            default:
                *(current++) = *(arg++);
        }
    }
    *(current++) = '"';
    *(current++) = '\0';
    oldresult = result; // save pointer in case realloc fails
    result = (char *) realloc(result, current - result);
    return result ? result : oldresult;
}

int SocketClient::sendMsg(const char *msg) {
    if (sendData(msg, strlen(msg) + 1) != 0) {
        return -1;
    }
    return 0;
}

int SocketClient::sendData(const void *data, int len) {
    struct iovec vec[1];
    vec[0].iov_base = const_cast<void *>(data);
    vec[0].iov_len = len;

    mWriteMutex.lock();
    int rc = sendDataLockedv(vec, 1);
    mWriteMutex.unlock();
    return rc;
}

int SocketClient::sendDatav(struct iovec *iov, int iovcnt) {
    mWriteMutex.lock();
    int rc = sendDataLockedv(iov, iovcnt);
    mWriteMutex.unlock();
    return rc;
}

int SocketClient::sendDataLockedv(struct iovec *iov, int iovcnt) {
    if (mSocket < 0) {
        errno = EHOSTUNREACH;
        return -1;
    }

    if (iovcnt <= 0) {
        return 0;
    }

    int ret = 0;
    int e = 0;
    int current = 0;

    struct sigaction new_action, old_action;
    memset(&new_action, 0, sizeof(new_action));
    new_action.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &new_action, &old_action);

    for (;;) {
        size_t rc = TEMP_FAILURE_RETRY(writev(mSocket, iov + current, iovcnt - current));

        if (rc > 0) {
            size_t written = rc;

            while (current < iovcnt && written >= iov[current].iov_len) {
                written -= iov[current].iov_len;
                current++;
            }

            //全部写完
            if (current == iovcnt) {
                break;;
            }

            iov[current].iov_base = (char*)iov[current].iov_base + written;
            iov[current].iov_len -= written;
            continue;
        }

        if (rc == 0) {
            e = EIO;
        } else {
            e = errno;
        }
        ret = -1;
        break;
    }

    sigaction(SIGPIPE, &old_action, &new_action);
    if (e != 0) {
        errno = e;
    }
    return ret;
}

void SocketClient::incRef() {
    ++mRefCount;
}

bool SocketClient::decRef() {
    bool deleteSelf = false;

    --mRefCount;

    if (mRefCount == 0) {
        deleteSelf = true;
    }

    if (deleteSelf) {
        delete this;
    }

    return deleteSelf;
}