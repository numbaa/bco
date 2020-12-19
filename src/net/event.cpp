#ifdef _WIN32
#include <WinSock2.h>
#else
#include <>
#endif // _WIN32
#include <exception>
#include <bco/utils.h>
#include "bco/net/event.h"

namespace bco {

namespace net {

Event::Event()
  : fd_listen_(::socket(AF_INET, SOCK_STREAM, 0)), fd_connect_(::socket(AF_INET, SOCK_STREAM, 0))
{
    if (fd_listen_ < 0 || fd_connect_ < 0) {
        throw std::exception { "create socket failed" };
    }
    set_non_block(fd_listen_);
    set_non_block(fd_connect_);
    addr_.sin_family = AF_INET;
    addr_.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
    int ret = ::bind(fd_listen_, reinterpret_cast<sockaddr*>(&addr_), sizeof(addr_));
    if (ret < 0)
        throw std::exception { "bind failed" };
    ret = ::listen(fd_listen_, 1);
    if (ret < 0)
        throw std::exception { "listen failed" };
    int len = sizeof(addr_);
    ret = ::getsockname(fd_listen_, reinterpret_cast<sockaddr*>(&addr_), &len);
    if (ret < 0)
        throw std::exception { "getsockname failed" };
}

int Event::fd()
{
    return fd_listen_;
}

void Event::emit()
{
    ::connect(fd_connect_, reinterpret_cast<sockaddr*>(&addr_), sizeof(addr_));
}

}

}