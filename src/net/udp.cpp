#include <bco/net/udp.h>

namespace bco {

namespace net {

template <SocketProactor P>
std::tuple<UdpSocket<P>, int> UdpSocket<P>::create(P* proactor, int family)
{
    int fd = proactor->create(family, SOCK_DGRAM);
    if (fd < 0)
        return { UdpSocket {}, -1 };
    else
        return { UdpSocket { proactor, family, fd }, 0 };
}

template <SocketProactor P>
UdpSocket<P>::UdpSocket(P* proactor, int family, int fd)
    : proactor_(proactor)
    , family_(family)
    , socket_(fd)
{
}

template <SocketProactor P>
ProactorTask<int> UdpSocket<P>::recv(std::span<std::byte> buffer)
{
    ProactorTask<int> task;
    int size = proactor_->read(socket_, buffer, [task](int length) mutable {
        if (task.await_ready())
            return;
        task.set_result(std::forward<int>(length));
        task.resume();
    });
    if (size > 0) {
        task.set_result(std::forward<int>(size));
    }
    return task;
}

template <SocketProactor P>
ProactorTask<std::tuple<int, SocketAddress>> UdpSocket<P>::recvfrom(std::span<::byte> buffer)
{
    ProactorTask<std::tuple<int, SocketAddress>> task;
    auto [size, remote_addr] = proactor_->recvfrom(socket_, buffer, [task](int length, const sockaddr_storage& remote_addr) mutable {
        if (task.await_ready())
            return;
        task.set_result(std::make_tuple(length, SocketAddress::from_storage(remote_addr)));
        task.resume();
    });
    if (size > 0) {
        task.set_result(std::make_tuple(size, SocketAddress::from_storage(remote_addr)));
    }
}

template <SocketProactor P>
int UdpSocket<P>::send(std::span<std::byte> buffer)
{
    return proactor_->write(buffer);
}

template <SocketProactor P>
int UdpSocket<P>::sendto(std::span<std::byte> buffer, const SocketAddress& addr)
{
    return proactor_->sendto(buffer, addr.to_storage());
}

template <SocketProactor P>
int UdpSocket<P>::bind(const SocketAddress& addr)
{
    return ::bind(socket_, reinterpret_cast<const sockaddr*>(&addr.to_storage()), sizeof(sockaddr_storage));
}

}

}