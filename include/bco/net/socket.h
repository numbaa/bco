#pragma once
#ifdef _WIN32
#include <WinSock2.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#endif // _WIN32
#include <bco/proactor.h>
#include <cassert>
#include <concepts>
#include <cstdint>
#include <span>

namespace bco {

namespace net {

template <typename T>
concept SocketProactor = bco::Proactor<T>&& requires(T p, int fd, uint32_t timeout_ms, sockaddr_in addr, std::span<std::byte> buff, std::function<void(int length)> cb)
{
    {
        p.read(fd, buff, cb)
    }
    ->std::same_as<int>;
    {
        p.write(fd, buff, cb)
    }
    ->std::same_as<int>;
    {
        p.accept(fd, cb)
    }
    ->std::same_as<int>;
    {
        p.connect(fd, addr, cb)
    }
    ->std::same_as<bool>;
    {
        p.create_fd()
    }
    ->std::same_as<int>;
};

template <SocketProactor P>
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

} // namespace net

} // namespace bco