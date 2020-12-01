#pragma once

#include "executor.h"
#include "task.h"
#include "proactor.h"

namespace bco {

template <Proactor P>
class TcpSocket {
public:
    static std::tuple<TcpSocket, int> create(P* proactor)
    {
        int sock = ::WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
        return { TcpSocket { proactor, sock }, sock < 0 ? WSAGetLastError() : 0 };
    }
    TcpSocket(P* proactor, int fd = -1)
    {
        if (proactor)
            proactor_->attach(fd);
    }
    [[nodiscard]] ProactorTask<int> read(std::span<std::byte> buffer)
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
    [[nodiscard]] ProactorTask<int> write(std::span<std::byte> buffer)
    {
        ProactorTask<int> task;
        int size = proactor_->write(socket_, buffer, [task](int length) mutable {
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
    [[nodiscard]] ProactorTask<TcpSocket> accept()
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
        }
        return task;
    }
    int listen()
    {
        return ::listen(socket_, 5);
    }
    int bind(sockaddr_in addr)
    {
        return ::bind(socket_, reinterpret_cast<sockaddr*>(&addr), sizeof(sockaddr_in));
    }

private:
    P* proactor_;
    int socket_;
};

/*
template <Proactor P>
inline TcpSocket<P> detail::default_value<TcpSocket<P>>()
{
    return TcpSocket<P> {nullptr, -1};
}
*/

}