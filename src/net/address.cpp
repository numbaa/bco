#ifdef _WIN32
#include <ws2ipdef.h>
#else
#include <>
#endif

#include <bco/net/address.h>

namespace bco {

namespace net {

Address::Address(in_addr ip, uint16_t port)
    : family_(AF_INET)
    , port_(port)
{
    ip_.v4 = ip;
}

Address::Address(in6_addr ip, uint16_t port)
    : family_(AF_INET6)
    , port_(port)
{
    ip_.v6 = ip;
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