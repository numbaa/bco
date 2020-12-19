#pragma once

namespace bco {

namespace net {

class Event {
public:
    Event();
    int fd();
    void emit();

private:
    int fd_listen_;
    int fd_connect_;
    sockaddr_in addr_ {};
};

}

}