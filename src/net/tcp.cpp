#include <bco/net/tcp.h>
#include <bco/net/proactor/select.h>
#ifdef _WIN32
#include <bco/net/proactor/iocp.h>
#else
#include <bco/net/proactor/epoll.h>
#endif // _WIN32

namespace bco {

namespace net {

template class TcpSocket<Select>;
#ifdef _WIN32
template class TcpSocket<IOCP>;
#else
template class TcpSocket<Epoll>;
#endif // _WIN32

template <SocketProactor P>
std::tuple<TcpSocket<P>, int> TcpSocket<P>::create(P* proactor, int family)
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
Task<int> TcpSocket<P>::recv(bco::Buffer buffer)
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

template <SocketProactor P>
Task<int> TcpSocket<P>::send(bco::Buffer buffer)
{
    Task<int> task;
    int size = proactor_->send(socket_, buffer, [task](int length) mutable {
        if (task.await_ready())
            return;
        task.set_result(std::forward<int>(length));
        task.resume();
    });
    if (size != 0) {
        task.set_result(std::forward<int>(size));
    }
    return task;
}

template <SocketProactor P>
Task<TcpSocket<P>> TcpSocket<P>::accept()
{
    Task<TcpSocket> task;
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
Task<int> TcpSocket<P>::connect(const Address& addr)
{
    Task<int> task;
    int ret = proactor_->connect(socket_, addr.to_storage(), [task](int ret) mutable {
        if (ret == 0) {
            task.set_result(0);
        } else if (ret < 0) {
            task.set_result(-errno);
        } else {
            assert(false);
        }
        task.resume();
    });
    if (ret == 0) {
        task.set_result(0);
    } else if (ret < 0) {
        task.set_result(-errno);
    } else {
        assert(false);
    }
    return task;
}

template <SocketProactor P>
int TcpSocket<P>::listen(int backlog)
{
    return proactor_->listen(socket_, backlog);
}

template <SocketProactor P>
int TcpSocket<P>::bind(const Address& addr)
{
    auto storage = addr.to_storage();
    return proactor_->bind(socket_, storage);
}

}

}