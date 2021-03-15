#pragma once
#include <WinSock2.h>
#include <Windows.h>
#include <functional>
#include <memory>
#include <vector>
#include <span>
#include <thread>
#include <mutex>

#include <bco/proactor.h>
#include <bco/net/address.h>
#include <bco/buffer.h>


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

    void start();
    void stop();

    int create(int domain, int type);
    int bind(int s, const sockaddr_storage& addr);
    int listen(int s, int backlog);

    int recv(int s, bco::Buffer buff, std::function<void(int)> cb);

    int recvfrom(int s, bco::Buffer buff, std::function<void(int, const sockaddr_storage&)> cb);

    int send(int s, bco::Buffer buff, std::function<void(int)> cb);
    int send(int s, bco::Buffer buff);

    int sendto(int s, bco::Buffer buff, const sockaddr_storage& addr);

    int accept(int listen_fd, std::function<void(int s)> cb);

    int connect(int s, const sockaddr_storage& addr, std::function<void(int)> cb);
    int connect(int s, const sockaddr_storage& addr);

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