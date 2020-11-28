#pragma once

#include "executor.h"
#include "task.h"
#include "proactor.h"

namespace bco {

class Proactor;

class TcpSocket {
public:
    static std::tuple<TcpSocket, int> create(Proactor* proactor);
    TcpSocket(Proactor* proactor, int fd = -1);
    [[nodiscard]] ProactorTask<int> read(Buffer buffer);
    [[nodiscard]] ProactorTask<int> write(Buffer buffer);
    [[nodiscard]] ProactorTask<TcpSocket> accept();
    int listen();
    int bind(sockaddr_in addr);

private:
    Proactor* proactor_;
    int socket_;
};

template <>
inline TcpSocket detail::default_value<TcpSocket>()
{
    return TcpSocket {nullptr, -1};
}

}