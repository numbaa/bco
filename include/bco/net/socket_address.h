#pragma once
#ifdef _WIN32
#include <WinSock2.h>
#include <in6addr.h>
#else
#endif
#include <cstdint>

namespace bco
{
    
namespace net
{
    
class SocketAddress {
public:
    SocketAddress() = default;
    SocketAddress(in_addr ip, uint16_t port);
    SocketAddress(in6_addr ip, uint16_t port);
    uint16_t port() const;
    in_addr ipv4() const;
    in6_addr ipv6() const;
    int family() const;

    void set_ip(in_addr ip);
    void set_ip(in6_addr ip);
    void set_port(uint16_t port);

    sockaddr_storage to_storage() const;
    static SocketAddress from_storage(const sockaddr_storage& storage);

private:
    int family_;
    uint16_t port_;
    union {
        in_addr v4;
        in6_addr v6;
    } ip_;
};

} // namespace net

} // namespace bco
