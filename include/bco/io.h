#pragma once

#include "executor.h"
#include "task.h"
#include "proactor.h"

namespace bco {

class Proactor;

class TcpSocket {
public:
    TcpSocket(Proactor& proactor);
    [[nodiscard]] Task<int> read();
    [[nodiscard]] Task<int> write();
    [[nodiscard]] Task<int> accept();
    int bind();

private:
    Proactor& proactor_;
};

}