#pragma once
#ifdef _WIN32
#include <WinSock2.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#endif // _WIN32
#include <concepts>
#include <cstdint>

#include <bco/buffer.h>
#include <bco/net/address.h>
#include <bco/proactor.h>

namespace bco {

namespace net {

template <typename T>
concept SocketProactor = bco::Proactor<T>&& requires(T p, int fd, int domain, int type, uint32_t timeout_ms,
    const sockaddr_storage& addr, bco::Buffer buff,
    std::function<void(int)> cb, int backlog,
    std::function<void(int, const sockaddr_storage&)> cb2)
{
    {
        p.recv(fd, buff, cb)
    }
    ->std::same_as<int>;

    {
        p.recvfrom(fd, buff, cb2)
    }
    ->std::same_as<int>;

    {
        p.send(fd, buff, cb)
    }
    ->std::same_as<int>;

    {
        p.send(fd, buff)
    }
    ->std::same_as<int>;

    {
        p.sendto(fd, buff, addr)
    }
    ->std::same_as<int>;

    {
        p.accept(fd, cb2)
    }
    ->std::same_as<int>;

    {
        p.connect(fd, addr, cb)
    }
    ->std::same_as<int>;

    {
        p.connect(fd, addr)
    }
    ->std::same_as<int>;

    {
        p.create(domain, type)
    }
    ->std::same_as<int>;

    //{
    //    p.bind(fd, addr)
    //}
    //->std::same_as<int>;

    //{
    //    p.listen(fd, backlog)
    //}
    //->std::same_as<int>;
};

} // namespace net

} // namespace bco