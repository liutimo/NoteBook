//
// Created by lz153 on 2020/6/6.
//

#ifndef ANDROIDNATIVE_SOCKETCLIENT_H
#define ANDROIDNATIVE_SOCKETCLIENT_H

#include <list>
#include <mutex>
#include <atomic>

class SocketClient {
    int             mSocket;
    bool            mSocketOwned;
    std::mutex      mWriteMutex;

    pid_t           mPid;
    uid_t           mUid;
    gid_t           mGid;

    std::atomic_int mRefCount;
    int             mCmdNum;
    bool            mUseCmdNum;
public:
    SocketClient(int sock, bool owned);
    SocketClient(int sock, bool owned, bool useCmdNum);
    virtual ~SocketClient();


    int getSocket() { return mSocket; }
    pid_t getPid() const { return mPid; }
    uid_t getUid() const { return mUid; }
    gid_t getGid() const { return mGid; }
    void setCmdNum(int cmdNum) {
        mCmdNum = cmdNum;
    }
    int getCmdNum() { return mCmdNum; }

    // Send null-terminated C strings:
    int sendMsg(int code, const char *msg, bool addErrno);
    int sendMsg(int code, const char *msg, bool addErrno, bool useCmdNum);
    int sendMsg(const char *msg);

    // Provides a mechanism to send a response code to the client.
    // Sends the code and a null character.
    int sendCode(int code);

    // Provides a mechanism to send binary data to client.
    // Sends the code and a null character, followed by 4 bytes of
    // big-endian length, and the data.
    int sendBinaryMsg(int code, const void *data, int len);

    // Sending binary data:
    int sendData(const void *data, int len);
    // iovec contents not preserved through call
    int sendDatav(struct iovec *iov, int iovcnt);

    // Optional reference counting.  Reference count starts at 1.  If
    // it's decremented to 0, it deletes itself.
    // SocketListener creates a SocketClient (at refcount 1) and calls
    // decRef() when it's done with the client.
    void incRef();
    bool decRef(); // returns true at 0 (but note: SocketClient already deleted)

    // return a new string in quotes with '\\' and '\"' escaped for "my arg"
    // transmissions
    static char *quoteArg(const char *arg);

private:
    void init(int socket, bool owned, bool useCmdNum);

    // Sending binary data. The caller should make sure this is protected
    // from multiple threads entering simultaneously.
    // returns 0 if successful, -1 if there is a 0 byte write or if any
    // other error occurred (use errno to get the error)
    int sendDataLockedv(struct iovec *iov, int iovcnt);
};

using SocketClientCollection =  std::list<SocketClient*> ;


#endif //ANDROIDNATIVE_SOCKETCLIENT_H
