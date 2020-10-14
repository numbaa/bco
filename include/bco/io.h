#pragma once

#include "executor.h"
#include "task.h"
#include "proactor.h"

namespace bco {

class Proactor;

class TcpSocket {
public:
    TcpSocket(Proactor* proactor, int fd);
    //TODO: should return customized awaitable, not just Task<>
    [[nodiscard]] IoTask<int> read(bco::Buffer buffer);
    [[nodiscard]] IoTask<int> write(bco::Buffer buffer);
    [[nodiscard]] IoTask<TcpSocket> accept();
    int bind();

private:
    Proactor* proactor_;
    int socket_;
};

}