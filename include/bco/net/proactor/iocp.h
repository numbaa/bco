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

class IOCP  {
public:
    class GetterSetter {
    public:
        IOCP* proactor() { return iocp_.get(); }
        IOCP* socket_proactor() { return iocp_.get(); }
        void set_socket_proactor(std::unique_ptr<IOCP>&& s)
        {
            iocp_ = std::move(s);
        }

    private:
        std::unique_ptr<IOCP> iocp_;
    };
public:
    IOCP();
    ~IOCP();
    int create_fd();
    void start();
    void stop();
    int read(int s, std::span<std::byte> buff, std::function<void(int length)> cb);

    int write(int s, std::span<std::byte> buff, std::function<void(int length)> cb);

    int accept(int listen_fd, std::function<void(int s)> cb);

    bool connect(int s, sockaddr_in addr, std::function<void(int)> cb);

    std::vector<PriorityTask> harvest_completed_tasks();

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