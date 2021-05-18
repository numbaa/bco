#pragma once
#ifdef _WIN32
#include <WinSock2.h>
#else // _WIN32
#include <netinet/in.h>
#endif

#include <array>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <bco/buffer.h>
#include <bco/executor.h>
#include <bco/net/address.h>
#include <bco/net/event.h>
#include <bco/proactor.h>

namespace bco {

namespace net {

class Select : public ProactorInterface {
    enum class Action : uint32_t {
        Recv,
        Send,
        Recvfrom,
        Accept,
        Connect,
    };
    struct SelectTask {
        int fd;
        Action action;
        bco::Buffer buff;
        std::function<void(int)> cb;
        std::function<void(int, const sockaddr_storage&)> cb2;
        #ifdef _WIN32
        void* recvmsg_func = nullptr;
        #endif
        SelectTask() = default;
        SelectTask(int _fd, Action _action, bco::Buffer _buff, std::function<void(int)> _cb);
        SelectTask(int _fd, Action _action, bco::Buffer _buff, std::function<void(int, const sockaddr_storage&)> _cb);
    };

public:
    Select();
    ~Select() override;

    void start(ExecutorInterface* executor);
    void start(std::unique_ptr<ExecutorInterface>&& executor);
    void stop();

    int create(int domain, int type);

    int recv(int s, bco::Buffer buff, std::function<void(int)> cb);

    int recvfrom(int s, bco::Buffer buff, std::function<void(int, const sockaddr_storage&)> cb, void* optdata = nullptr);

    int send(int s, bco::Buffer buff, std::function<void(int)> cb);
    int send(int s, bco::Buffer buff);

    int sendto(int s, bco::Buffer buff, const sockaddr_storage& addr, void* optdata = nullptr);

    int accept(int s, std::function<void(int, const sockaddr_storage&)> cb);

    int connect(int s, const sockaddr_storage& addr, std::function<void(int)> cb);
    int connect(int s, const sockaddr_storage& addr);

    std::vector<PriorityTask> harvest() override;

private:
    void do_io();
    int send_sync(int s, bco::Buffer buff, std::function<void(int)> cb);
    int send_async(int s, bco::Buffer buff, std::function<void(int)> cb);
    void do_accept(const SelectTask& task);
    void do_recv(const SelectTask& task);
    void do_recvfrom(const SelectTask& task);
    void do_send(const SelectTask& task);
    void on_connected(const SelectTask& task);
    void on_io_event(const std::map<int, SelectTask>& tasks, const fd_set& fds);
    std::tuple<std::map<int, SelectTask>, std::map<int, SelectTask>> get_pending_io();
    static void prepare_fd_set(const std::map<int, SelectTask>& tasks, fd_set& fds);
    void wake();

private:
    Event stop_event_;
    Event wakeup_event_;
    std::mutex mtx_;
    int max_rfd_ {};
    int max_wfd_ {};
    std::map<int, SelectTask> pending_rfds_;
    std::map<int, SelectTask> pending_wfds_;
    std::vector<PriorityTask> completed_task_;
    fd_set rfds_ {};
    fd_set wfds_ {};
    fd_set efds_ {};
    bool started_ { false };
    ExecutorInterface* io_executor_;
    std::unique_ptr<ExecutorInterface> io_executor_holder_;
    timeval timeout_;
    std::atomic<bool> polling_ { false };
};

} // namespace net

} // namespace bco