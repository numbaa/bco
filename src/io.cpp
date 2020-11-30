#include <WinSock2.h>
#include <cassert>
#include <bco/io.h>
#include <bco/buffer.h>

namespace bco {

std::tuple<TcpSocket, int> TcpSocket::create(Proactor* proactor)
{
    int sock = ::socket(AF_INET, SOCK_STREAM, 0);
    //change -1 to something like WSAGetLastError()
    return { TcpSocket { proactor, sock }, sock < 0 ? -1 : 0 };
}

TcpSocket::TcpSocket(Proactor* proactor, int fd)
    : proactor_(proactor), socket_(fd)
{
    if (proactor)
        proactor_->attach(fd);
}

ProactorTask<int> TcpSocket::read(Buffer buffer)
{
    ProactorTask<int> task;
    int size = proactor_->read(socket_, buffer, [task](int length) mutable {
        task.set_result(std::forward<int>(length));
        task.resume();
    });
    /*
    if (size > 0) {
        task.set_result(std::forward<int>(size));
    }
    */
    return task;
}

ProactorTask<int> TcpSocket::write(Buffer buffer)
{
    ProactorTask<int> task;
    int size = proactor_->write(socket_, buffer, [task](int length) mutable {
        task.set_result(std::forward<int>(length));
        task.resume();
    });
    /*
    if (size > 0) {
        task.set_result(std::forward<int>(size));
    }
    */
    return task;
}

ProactorTask<TcpSocket> TcpSocket::accept()
{
    ProactorTask<TcpSocket> task;
    auto proactor = proactor_;
    int fd = proactor_->accept(socket_, [task, proactor](int fd) mutable {
        TcpSocket s{proactor, fd};
        task.set_result(std::move(s));
        task.resume();
    });
    if (fd > 0) {
        TcpSocket s{proactor, fd};
        task.set_result(std::move(s));
    } else if (fd < 0) {
        assert(false);
    }
    return task;
}

int TcpSocket::bind(sockaddr_in addr)
{
    return ::bind(socket_, reinterpret_cast<sockaddr*>(&addr), sizeof(sockaddr_in));
}

int TcpSocket::listen()
{
    return ::listen(socket_, 5);
}

} // namespace bco
