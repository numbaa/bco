#pragma once
#ifdef _WIN32
// clang-format off
#include <Windows.h>
#include <WinSock2.h>
// clang-format on
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <thread>
#include <vector>

#include <bco/buffer.h>
#include <bco/net/address.h>
#include <bco/proactor.h>

namespace bco {

namespace net {

class IOCP : public ProactorInterface {

public:
    IOCP();
    ~IOCP() override;

    void start();
    void stop();

    int create(int domain, int type);

    int recv(int s, bco::Buffer buff, std::function<void(int)> cb);

    int recvfrom(int s, bco::Buffer buff, std::function<void(int, const sockaddr_storage&)> cb, void* optdata = nullptr);

    int send(int s, bco::Buffer buff, std::function<void(int)> cb);
    int send(int s, bco::Buffer buff);

    int sendto(int s, bco::Buffer buff, const sockaddr_storage& addr, void* optdata = nullptr);

    int accept(int listen_fd, std::function<void(int, const sockaddr_storage&)> cb);

    int connect(int s, const sockaddr_storage& addr, std::function<void(int)> cb);
    int connect(int s, const sockaddr_storage& addr);

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

} // namespace net

} // namespace bco

#endif // ifdef _WIN32