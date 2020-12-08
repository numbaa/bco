#pragma once
#include <Windows.h>
#include <functional>
#include <memory>
#include <vector>
#include <span>
#include <thread>
#include <mutex>


namespace bco {

class IOCP {
public:
    IOCP();
    int create_fd();
    void start();
    void stop();
    int read(int s, std::span<std::byte> buff, std::function<void(int length)>&& cb);

    int write(int s, std::span<std::byte> buff, std::function<void(int length)>&& cb);

    int accept(int listen_fd, std::function<void(int s)>&& cb);

    bool connect(int s, sockaddr_in addr, std::function<void(int)>&& cb);

    std::vector<std::function<void()>> drain(uint32_t timeout_ms);

private:
    void iocp_loop();
    DWORD next_timeout();
    void handle_overlap_success(WSAOVERLAPPED* overlapped, int bytes);

private:
    std::mutex mtx_;
    std::thread harvest_thread_;
    std::vector<std::function<void()>> completed_tasks_;
    ::HANDLE complete_port_;
};

}