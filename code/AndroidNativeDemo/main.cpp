#include <iostream>

#include "RefBase.h"
#include <sys/epoll.h>

class A : public RefBase {
public:
    A() {
        std::cout << "A()" << std::endl;
    }

    ~A() {
        std::cout << "~A()" <<std::endl;
    }
};


int main() {

    struct epoll_event eventItem;

    return 0;
}
