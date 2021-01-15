#include <bco/net/tcp.h>

namespace bco {

namespace net {

template <SocketProactor P>
static std::tuple<TcpSocket<P>, int> TcpSocket<P>::create(P* proactor, int family)
{
    int fd = proactor->create(family, SOCK_STREAM);
    if (fd < 0)
        return { TcpSocket {}, -1 };
    else
        return { TcpSocket { proactor, family, fd }, 0 };
}

template <SocketProactor P>
TcpSocket<P>::TcpSocket(P* proactor, int family, int fd)
    : proactor_(proactor)
    , family_(family)
    , socket_(fd)
{
}

template <SocketProactor P>
ProactorTask<int> TcpSocket<P>::recv(std::span<std::byte> buffer)
{
    ProactorTask<int> task;
    int size = proactor_->recv(socket_, buffer, [task](int length) mutable {
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
ProactorTask<int> TcpSocket<P>::send(std::span<std::byte> buffer)
{
    ProactorTask<int> task;
    int size = proactor_->send(socket_, buffer, [task](int length) mutable {
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
ProactorTask<TcpSocket<P>> TcpSocket<P>::accept()
{
    ProactorTask<TcpSocket> task;
    auto proactor = proactor_;
    int fd = proactor_->accept(socket_, [task, proactor](int fd) mutable {
        if (task.await_ready())
            return;
        TcpSocket s { proactor, fd };
        task.set_result(std::move(s));
        task.resume();
    });
    if (fd > 0) {
        TcpSocket s { proactor, fd };
        task.set_result(std::move(s));
    } else if (fd < 0) {
        assert(false);
    }
    return task;
}

template <SocketProactor P>
ProactorTask<int> TcpSocket<P>::connect(const Address& addr)
{
}

template <SocketProactor P>
int TcpSocket<P>::listen(int backlog)
{
    return ::listen(socket_, backlog);
}

template <SocketProactor P>
int TcpSocket<P>::bind(const Address& addr)
{
    return ::bind(socket_, reinterpret_cast<sockaddr*>(&addr.to_storage()), sizeof(sockaddr_storage));
}

}

}