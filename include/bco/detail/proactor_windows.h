#pragma once

#include <functional>
#include <memory>
#include <vector>
#include <Windows.h>

#include <bco/buffer.h>
#include <bco/executor.h>

namespace bco {

class Proactor {
public:
    Proactor();
    int read(SOCKET s, Buffer buff, std::function<void(int length)>&& cb);
    int write(SOCKET s, Buffer buff, std::function<void(int length)>&& cb);
    int accept(SOCKET s, std::function<void(SOCKET s)>&& cb);
    bool connect(SOCKADDR_IN& addr, std::function<void(SOCKET)>&& cb);
    std::vector<std::function<void(SOCKET s)>> drain(uint32_t timeout_ms);

private:
    void set_executor(Executor* executor);

private:
    ::HANDLE complete_port_;
};

}