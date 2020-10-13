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
    Proactor(Executor& context);
    int read(SOCKET s, Buffer buff, std::function<void(int length)>&& cb);
    int write(SOCKET s, Buffer buff, std::function<void(int length)>&& cb);
    int accept(SOCKET s, std::function<void(SOCKET s)>&& cb);
    bool connect(SOCKADDR_IN& addr, std::function<void()>&& cb);
    std::vector<std::function<void()>> drain(uint32_t timeout_ms);

private:
    ::HANDLE complete_port_;
    Executor& executor_;
};

}