#pragma once

#include "context.h"
#include "task.h"
#include "proactor.h"

namespace bco {

class Proactor;

class TcpSocket {
public:
    TcpSocket(Proactor* proactor);
    //TODO: should return customized awaitable, not just Task<>
    [[nodiscard]] IoTask<int> read(bco::Buffer buffer);
    [[nodiscard]] IoTask<int> write(bco::Buffer buffer);
    [[nodiscard]] IoTask<TcpSocket> accept();
    int bind();

private:
    int socket_;
    Proactor* proactor_;
};

}