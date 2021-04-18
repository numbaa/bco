#include <bco/net/udp.h>
#include <bco/net/proactor/select.h>
#ifdef _WIN32
#include <bco/net/proactor/iocp.h>
#else
#include <bco/net/proactor/epoll.h>
#endif // _WIN32
#include "../common.h"


namespace bco {

namespace net {

template class UdpSocket<Select>;
#ifdef _WIN32
template class UdpSocket<IOCP>;
#else
template class UdpSocket<Epoll>;
#endif // _WIN32

template <SocketProactor P>
std::tuple<UdpSocket<P>, int> UdpSocket<P>::create(P* proactor, int family)
{
    int fd = proactor->create(family, SOCK_DGRAM);
    if (fd < 0) {
        return { UdpSocket {}, -1 };
    }
#ifdef _WIN32
    auto func = get_recvmsg_func(fd);
    if (func == nullptr) {
        close_socket(fd);
        return { UdpSocket {}, -1 };
    }
    auto udp_socket = UdpSocket { proactor, family, fd };
    udp_socket.set_recvmsg_func(func);
    return { udp_socket, 0 };
#else
    return { UdpSocket { proactor, family, fd }, 0 };
#endif // _WIN32
}

template <SocketProactor P>
UdpSocket<P>::UdpSocket(P* proactor, int family, int fd)
    : proactor_(proactor)
    , family_(family)
    , socket_(fd)
{
}

template <SocketProactor P>
Task<int> UdpSocket<P>::recv(bco::Buffer buffer)
{
    Task<int> task;
    int error = proactor_->recv(socket_, buffer, [task](int length) mutable {
        if (task.await_ready())
            return;
        task.set_result(std::forward<int>(length));
        task.resume();
    });
    if (error < 0) {
        task.set_result(std::forward<int>(error));
    }
    return task;
}

//FIXME: support select
template <SocketProactor P>
Task<std::tuple<int, Address>> UdpSocket<P>::recvfrom(bco::Buffer buffer)
{
    Task<std::tuple<int, Address>> task;
    auto error = proactor_->recvfrom(socket_, buffer, [task](int length, const sockaddr_storage& remote_addr) mutable {
        if (task.await_ready())
            return;
        task.set_result(std::make_tuple(length, Address::from_storage(remote_addr)));
        task.resume();
    });
    if (error < 0) {
        task.set_result(std::make_tuple(error, Address{}));
    }
    return task;
}

template <SocketProactor P>
int UdpSocket<P>::send(bco::Buffer buffer)
{
    return proactor_->send(socket_, buffer);
}

template <SocketProactor P>
int UdpSocket<P>::sendto(bco::Buffer buffer, const Address& addr)
{
    return proactor_->sendto(socket_, buffer, addr.to_storage());
}

template <SocketProactor P>
int UdpSocket<P>::bind(const Address& addr)
{
    auto storage = addr.to_storage();
    return bind_socket(socket_, storage);
}

template <SocketProactor P>
int UdpSocket<P>::connect(const Address& addr)
{
    auto storage = addr.to_storage();
    return proactor_->connect(socket_, storage);
}

template <SocketProactor P>
void UdpSocket<P>::close()
{
    close_socket(socket_);
}

} // namespace net

} // namespace bco
