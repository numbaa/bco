#pragma once
#ifdef _WIN32
#include <WinSock2.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#endif // _WIN32
#include <cassert>
#include <concepts>
#include <cstdint>
#include <span>

#include <bco/proactor.h>
#include <bco/net/socket_address.h>

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
    static std::tuple<TcpSocket, int> create(P* proactor, int family)
    {
        int fd = proactor->create_fd(family, SOCK_STREAM);
        if (fd < 0)
            return { TcpSocket {}, -1 };
        else
            return { TcpSocket { proactor, family, fd }, 0 };
    }
    TcpSocket() = default;
    TcpSocket(P* proactor, int family, int fd = -1)
        : proactor_(proactor)
        , family_(family)
        , socket_(fd)
    {
    }
    [[nodiscard]] ProactorTask<int> recv(std::span<std::byte> buffer)
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
    [[nodiscard]] ProactorTask<int> send(std::span<std::byte> buffer)
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
    int family_;
    int socket_;
};

template <SocketProactor P>
class UdpSocket {
public:
    static std::tuple<TcpSocket, int> create(P* proactor, int family)
    {
        int fd = proactor->create_fd(family, SOCK_DGRAM);
        if (fd < 0)
            return { UdpSocket {}, -1 };
        else
            return { UdpSocket { proactor, family, fd }, 0 };
    }
    UdpSocket() = default;
    UdpSocket(P* proactor, int family, int fd = -1)
        : proactor_(proactor)
        , family_(family),
        , socket_(fd)
    {
    }
    [[nodiscard]] ProactorTask<int> recv(std::span<std::byte> buffer)
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
    [[nodiscaard]] ProactorTask<std::tuple<int, SocketAddress>> recvfrom(std::span<::byte> buffer)
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
    int send(std::span<std::byte> buffer)
    {
        return proactor_->write(buffer);
    }
    int sendto(std::span<std::byte> buffer, const SocketAddress& addr)
    {
        return proactor_->sendto(buffer, addr.to_storage());
    }
    int bind(const SocketAddress& addr)
    {
        return ::bind(socket_, reinterpret_cast<const sockaddr*>(&addr.to_storage()), sizeof(sockaddr_storage));
    }
    
private:
    P* proactor_;
    int family_;
    int socket_;
};

} // namespace net

} // namespace bco