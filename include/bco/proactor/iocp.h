#pragma once
#include <Windows.h>
#include <functional>
#include <memory>
#include <vector>
#include <span>


namespace bco {
class SimpleExecutor;
//class Buffer;

class IOCP {
public:
    IOCP();

    int read(int s, std::span<std::byte> buff, std::function<void(int length)>&& cb);

    int write(int s, std::span<std::byte> buff, std::function<void(int length)>&& cb);

    int accept(int s, std::function<void(int s)>&& cb);

    bool connect(sockaddr_in addr, std::function<void(int)>&& cb);

    std::vector<std::function<void()>> drain(uint32_t timeout_ms);

    void attach(int fd);

private:
    void set_executor(SimpleExecutor* executor);

private:
    ::HANDLE complete_port_;
};

}