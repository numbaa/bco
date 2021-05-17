#pragma once
#include <array>

namespace bco {

namespace net {

class Event {
public:
    Event();
    int fd();
    void emit();
    void reset();

private:
    int rfd_;
    int wfd_;
    sockaddr_in addr_ {};
    std::array<char, 1024> buff_;
};

} // namespace net

} // namespace bco
