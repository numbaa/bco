#pragma once
#include <cassert>

#include <bco/net/socket.h>
#include <bco/coroutine/task.h>

namespace bco {

namespace net {

template <SocketProactor P>
class TcpSocket {
public:
    static std::tuple<TcpSocket, int> create(P* proactor, int family);

    TcpSocket() = default;
    TcpSocket(P* proactor, int family, int fd = -1);

    [[nodiscard]] Task<int> recv(std::span<std::byte> buffer);
    [[nodiscard]] Task<int> send(std::span<std::byte> buffer);
    [[nodiscard]] Task<TcpSocket> accept();
    [[nodiscard]] Task<int> connect(const Address& addr);
    int listen(int backlog);
    int bind(const Address& addr);

private:
    P* proactor_;
    int family_;
    int socket_;
};

}

}