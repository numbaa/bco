#pragma once
#include <liburing.h>

#include "executor.h"
#include "task.h"

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

class Proactor {
public:
    Proactor(Executor& executor);
    void read(std::function<void()>);
    void write(std::function<void()>);
    void accept(std::function<void()>);

private:
    ::io_uring iouring_;
    Executor& executor_;
};

}