#pragma once
#include <bco/net/socket.h>
#include <bco/coroutine/task.h>

namespace bco {

namespace net {

template <SocketProactor P>
class UdpSocket {
public:
    static std::tuple<UdpSocket, int> create(P* proactor, int family);

    UdpSocket() = default;
    UdpSocket(P* proactor, int family, int fd = -1);

    [[nodiscard]] ProactorTask<int> recv(std::span<std::byte> buffer);
    [[nodiscard]] ProactorTask<std::tuple<int, Address>> recvfrom(std::span<::byte> buffer);
    int connect(const Address& addr);
    int send(std::span<std::byte> buffer);
    int sendto(std::span<std::byte> buffer, const Address& addr);
    int bind(const Address& addr);

private:
    P* proactor_;
    int family_;
    int socket_;
};

}

}