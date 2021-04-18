#pragma once
#include <cassert>

#include <bco/net/socket.h>
#include <bco/coroutine/task.h>

namespace bco {

namespace net {

template <SocketProactor P>
class TcpSocket {
public:
    enum class Shutdown : int {
        Receive = 0,
        Send = 1,
        Both = 2,
    };

public:
    static std::tuple<TcpSocket, int> create(P* proactor, int family);

    TcpSocket() = default;
    TcpSocket(P* proactor, int family, int fd = -1);

    [[nodiscard]] Task<int> recv(bco::Buffer buffer);
    [[nodiscard]] Task<int> send(bco::Buffer buffer);
    [[nodiscard]] Task<std::tuple<TcpSocket<P>, Address>> accept();
    [[nodiscard]] Task<int> connect(const Address& addr);
    int listen(int backlog);
    int bind(const Address& addr);
    void shutdown(Shutdown how);
    void close();

private:
    P* proactor_;
    int family_;
    int socket_;
};

}

}