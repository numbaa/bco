#pragma once

#include "executor.h"
#include "task.h"
#include "proactor.h"

namespace bco {

class Proactor;

class TcpSocket {
public:
    TcpSocket(Proactor* proactor, int fd = -1);
    //TODO: should return customized awaitable, not just Task<>
    [[nodiscard]] ProactorTask<int> read(bco::Buffer buffer);
    [[nodiscard]] ProactorTask<int> write(bco::Buffer buffer);
    [[nodiscard]] ProactorTask<TcpSocket> accept();
    int bind();

private:
    Proactor* proactor_;
    int socket_;
};

}