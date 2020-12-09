#pragma once
#include <Windows.h>
#include <functional>
#include <memory>
#include <vector>
#include <span>
#include <thread>
#include <mutex>
#include <bco/proactor.h>


namespace bco {

namespace net {

class IOCP : public bco::ProactorInterface {
public:
    IOCP();
    int create_fd();
    void start();
    void stop();
    int read(int s, std::span<std::byte> buff, std::function<void(int length)>&& cb);

    int write(int s, std::span<std::byte> buff, std::function<void(int length)>&& cb);

    int accept(int listen_fd, std::function<void(int s)>&& cb);

    bool connect(int s, sockaddr_in addr, std::function<void(int)>&& cb);

    std::vector<PriorityTask> harvest_completed_tasks() override;

private:
    void iocp_loop();
    DWORD next_timeout();
    void handle_overlap_success(WSAOVERLAPPED* overlapped, int bytes);

private:
    std::mutex mtx_;
    std::thread harvest_thread_;
    std::vector<PriorityTask> completed_tasks_;
    ::HANDLE complete_port_;
};

}

}