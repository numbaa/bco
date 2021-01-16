#ifdef _WIN32
#include <WinSock2.h>
#include <Windows.h>
#include <WS2tcpip.h>
#else
#include <>
#endif

#include <bco/net/address.h>

namespace bco {

namespace net {

Address::Address(in_addr ip, uint16_t port)
    : family_(AF_INET)
    , port_(port)
    , ip_(ip)
{
}

Address::Address(const in6_addr& ip, uint16_t port)
    : family_(AF_INET6)
    , port_(port)
    , ip_(ip)
{
}

Address::Address(const sockaddr_in& addr)
    : family_(AF_INET)
    , port_(addr.sin_port)
    , ip_(addr.sin_addr)
{
}

Address::Address(const sockaddr_in6& addr)
    : family_(AF_INET)
    , port_(addr.sin6_port)
    , ip_(addr.sin6_addr)
{
}

uint16_t Address::port() const
{
    return port_;
}

in_addr Address::ipv4() const
{
    return ip_.v4;
}

in6_addr Address::ipv6() const
{
    return ip_.v6;
}

int Address::family() const
{
    return family_;
}

void Address::set_ip(in_addr ip)
{
    ip_.v4 = ip;
}

void Address::set_ip(in6_addr ip)
{
    ip_.v6 = ip;
}

void Address::set_port(uint16_t port)
{
    port_ = port;
}

bool Address::is_private() const
{
    return is_linklocal() || is_loopback() || is_private_network() || is_shared_network();
}

bool Address::is_loopback() const
{
    if (family_ == AF_INET) {
        uint32_t ip = ::ntohl(ip_.v4.s_addr);
        return (ip >> 24) == 127;
    } else if (family_ == AF_INET6) {
        return ::memcmp(&ip_.v6, &in6addr_loopback, sizeof(ip_.v6)) == 0;
    }
    return false;
}

bool Address::is_linklocal() const
{
    if (family_ == AF_INET) {
        uint32_t ip = ::ntohl(ip_.v4.s_addr);
        return ((ip >> 16) == ((169 << 8) | 254));
    } else if (family_ == AF_INET6) {
        return (ip_.v6.s6_addr[0] == 0xFE) && ((ip_.v6.s6_addr[1] & 0xC0) == 0x80);
    }
    return false;
}

//192.168.xx.xx
//172.[16-31].xx.xx
//10.xx.xx.xx
//fd:xx...
bool Address::is_private_network() const
{
    if (family_ == AF_INET) {
        uint32_t ip = ::ntohl(ip_.v4.s_addr);
        return ((ip >> 24) == 10)
            || ((ip >> 20) == ((172 << 4) | 1))
            || ((ip >> 16) == ((192 << 8) | 168));
    } else if (family_ == AF_INET6) {
        return ip_.v6.s6_addr[0] == 0xFD;
    }
    return false;
}

//100.64.xx.xx
bool Address::is_shared_network() const
{
    if (family_ != AF_INET) {
        return false;
    }
    uint32_t ip = ::ntohl(ip_.v4.s_addr);
    return (ip >> 22) == ((100 << 2) | 1);
}

sockaddr_storage Address::to_storage() const
{
    sockaddr_storage storage {};
    if (family_ == AF_INET) {
        sockaddr_in* addr = reinterpret_cast<sockaddr_in*>(&storage);
        addr->sin_family = AF_INET;
        addr->sin_port = port_;
        addr->sin_addr = ip_.v4;
    } else if (family_ == AF_INET6) {
        sockaddr_in6* addr = reinterpret_cast<sockaddr_in6*>(&storage);
        addr->sin6_family = AF_INET6;
        addr->sin6_port = port_;
        addr->sin6_addr = ip_.v6;
    }
    return storage;
}

Address Address::from_storage(const sockaddr_storage& storage)
{
    Address addr {};
    if (storage.ss_family == AF_INET) {
        const sockaddr_in* in4 = reinterpret_cast<const sockaddr_in*>(&storage);
        addr.family_ = AF_INET;
        addr.port_ = in4->sin_port;
        addr.ip_.v4 = in4->sin_addr;
    } else if (storage.ss_family == AF_INET6) {
        const sockaddr_in6* in6 = reinterpret_cast<const sockaddr_in6*>(&storage);
        addr.family_ = AF_INET6;
        addr.port_ = in6->sin6_port;
        addr.ip_.v6 = in6->sin6_addr;
    }
    return addr;
}

}

}