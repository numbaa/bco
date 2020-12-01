#pragma once
#include <cassert>
#include "executor.h"
#include "task.h"
#include "proactor.h"

namespace bco {

template <Proactor P>
class TcpSocket {
public:
    static std::tuple<TcpSocket, int> create(P* proactor)
    {
        int sock = static_cast<int>(::socket(AF_INET, SOCK_STREAM, 0));
        return { TcpSocket { proactor, sock }, sock < 0 ? -1 : 0 };
    }
    TcpSocket() = default;
    TcpSocket(P* proactor, int fd = -1)
        : proactor_(proactor)
        , socket_(fd)
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
        /*
        if (size > 0) {
            task.set_result(std::forward<int>(size));
        }
        */
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
        /*
        if (size > 0) {
            task.set_result(std::forward<int>(size));
        }
        */
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
        } else if (fd < 0) {
            assert(false);
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
    static TcpSocket defalt_value()
    {
        return TcpSocket { nullptr, -1 };
    }

private:
    P* proactor_;
    int socket_;
};

}