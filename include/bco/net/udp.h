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

    [[nodiscard]] Task<int> recv(bco::Buffer buffer);
    [[nodiscard]] Task<std::tuple<int, Address>> recvfrom(bco::Buffer buffer);
    int connect(const Address& addr);
    int send(bco::Buffer buffer);
    int sendto(bco::Buffer, const Address& addr);
    int bind(const Address& addr);

private:
    P* proactor_;
    int family_;
    int socket_;
};

}

}