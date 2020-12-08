#pragma once
#include <cassert>
#include <bco/proactor.h>
#include <bco/task.h>

namespace bco {

template <Proactor P>
class TcpSocket {
public:
    static std::tuple<TcpSocket, int> create(P* proactor)
    {
        int fd = proactor->create_fd();
        if (fd < 0)
            return { TcpSocket {}, -1 };
        else
            return { TcpSocket { proactor, fd }, 0 };
    }
    TcpSocket() = default;
    TcpSocket(P* proactor, int fd = -1)
        : proactor_(proactor)
        , socket_(fd)
    {
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