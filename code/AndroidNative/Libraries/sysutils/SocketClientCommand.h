//
// Created by lz153 on 2020/6/6.
//

#ifndef ANDROIDNATIVE_SOCKETCLIENTCOMMAND_H
#define ANDROIDNATIVE_SOCKETCLIENTCOMMAND_H
class SocketClientCommand {
public:
    virtual ~SocketClientCommand() { }
    virtual void runSocketCommand(SocketClient *client) = 0;
};
#endif //ANDROIDNATIVE_SOCKETCLIENTCOMMAND_H
