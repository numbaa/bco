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
    int read(int s, Buffer buff, std::function<void(int length)>&& cb);
    int write(int s, Buffer buff, std::function<void(int length)>&& cb);
    int accept(int s, std::function<void(int s)>&& cb);
    bool connect(sockaddr_in addr, std::function<void(int)>&& cb);
    std::vector<std::function<void(int/*error*/, int/*sock*/)>> drain(uint32_t timeout_ms);

private:
    void set_executor(Executor* executor);

private:
    ::HANDLE complete_port_;
};

}