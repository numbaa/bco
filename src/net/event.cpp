#ifdef _WIN32
#include <WinSock2.h>
#else
#include <sys/socket.h>
//#include <netinet/in.h>
#include <arpa/inet.h>
#endif // _WIN32
#include <bco/exception.h>
#include <bco/net/event.h>
#include <bco/utils.h>
#include "../common.h"

namespace bco {

namespace net {

Event::Event()
    : rfd_(static_cast<int>(::socket(AF_INET, SOCK_DGRAM, 0)))
    , wfd_(static_cast<int>(::socket(AF_INET, SOCK_DGRAM, 0)))
{
    if (rfd_ < 0 || wfd_ < 0) {
        throw NetworkException { "create socket failed" };
    }
    set_non_block(rfd_);
    set_non_block(wfd_);
    addr_.sin_family = AF_INET;
    addr_.sin_addr = bco::to_in_addr("127.0.0.1");
    addr_.sin_port = 0;
    int ret = ::bind(rfd_, reinterpret_cast<sockaddr*>(&addr_), sizeof(addr_));
    if (ret < 0)
        throw NetworkException { "bind failed" };
#ifdef _WIN32
    int len = sizeof(addr_);
#else
    socklen_t len = sizeof(addr_);
#endif
    ret = ::getsockname(rfd_, reinterpret_cast<sockaddr*>(&addr_), &len);
    if (ret < 0)
        throw NetworkException { "getsockname failed" };
    ret = ::connect(wfd_, reinterpret_cast<sockaddr*>(&addr_), len);
    if (ret < 0)
        throw NetworkException { "connect failed" };
}

int Event::fd()
{
    return rfd_;
}

void Event::emit()
{
    ::sendto(wfd_, "x", 1, 0, reinterpret_cast<sockaddr*>(&addr_), sizeof(addr_));
}

void Event::reset()
{
    sockaddr_in addr;
#ifdef _WIN32
    int len = sizeof(addr);
#else
    socklen_t len = sizeof(addr);
#endif // _WIN32
    ::recvfrom(static_cast<int>(rfd_), buff_.data(), static_cast<int>(buff_.size()), 0, reinterpret_cast<sockaddr*>(&addr), &len);
}

} // namespace net

} // namespace bco