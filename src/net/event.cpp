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

namespace bco {

namespace net {

Event::Event()
    : fd_listen_(::socket(AF_INET, SOCK_STREAM, 0))
    , fd_connect_(::socket(AF_INET, SOCK_STREAM, 0))
{
    if (fd_listen_ < 0 || fd_connect_ < 0) {
        throw NetworkException { "create socket failed" };
    }
    set_non_block(fd_listen_);
    set_non_block(fd_connect_);
    addr_.sin_family = AF_INET;
    inet_aton("127.0.0.1", &addr_.sin_addr);
    int ret = ::bind(fd_listen_, reinterpret_cast<sockaddr*>(&addr_), sizeof(addr_));
    if (ret < 0)
        throw NetworkException { "bind failed" };
    ret = ::listen(fd_listen_, 1);
    if (ret < 0)
        throw NetworkException { "listen failed" };
#ifdef _WIN32
    int len = sizeof(addr_);
#else
    socklen_t len = sizeof(addr_);
#endif
    ret = ::getsockname(fd_listen_, reinterpret_cast<sockaddr*>(&addr_), &len);
    if (ret < 0)
        throw NetworkException { "getsockname failed" };
}

int Event::fd()
{
    return fd_listen_;
}

void Event::emit()
{
    ::connect(fd_connect_, reinterpret_cast<sockaddr*>(&addr_), sizeof(addr_));
}

} // namespace net

} // namespace bco