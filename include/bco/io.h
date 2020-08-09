#pragma once

#include "context.h"
#include "task.h"
#include "proactor.h"

namespace bco {

class Proactor;

class TcpSocket {
public:
    TcpSocket(Proactor* proactor);
    [[nodiscard]] Task<int> read(std::shared_ptr<std::vector<uint8_t>> buffer);
    [[nodiscard]] Task<int> write(std::shared_ptr<std::vector<uint8_t>> buffer);
    [[nodiscard]] Task<int> accept();
    int bind();

private:
    Proactor* proactor_;
};

}