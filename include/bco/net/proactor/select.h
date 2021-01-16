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
#include <span>
#include <thread>
#include <vector>

#include <bco/net/event.h>
#include <bco/proactor.h>
#include <bco/net/address.h>

namespace bco {

namespace net {

class Select {
    enum class Action : uint32_t {
        Read,
        Write,
        Accept,
        Connect,
    };
    struct SelectTask {
        int fd;
        Action action;
        std::span<std::byte> buff;
        std::function<void(int)> cb;
    };

public:
    class GetterSetter {
    public:
        Select* proactor() { return select_.get(); }
        Select* socket_proactor() { return select_.get(); }
        void set_socket_proactor(std::unique_ptr<Select>&& s)
        {
            select_ = std::move(s);
        }

    private:
        std::unique_ptr<Select> select_;
    };

public:
    Select();
    ~Select();

    void start();
    void stop();

    int create(int domain, int type);
    int bind(int s, const sockaddr_storage& addr);
    int listen(int s, int backlog);

    int recv(int s, std::span<std::byte> buff, std::function<void(int)> cb);

    std::tuple<int, Address> recvfrom(int s, std::span<std::byte> buff, std::function<void(int, const Address&)>);

    int send(int s, std::span<std::byte> buff, std::function<void(int)> cb);
    int send(int s, std::span<std::byte> buff);

    int sendto(int s, std::span<std::byte> buff, const sockaddr_storage& addr, std::function<void(int)> cb);
    int sendto(int s, std::span<std::byte> buff, const sockaddr_storage& addr);

    int accept(int s, std::function<void(int)> cb);

    int connect(int s, const sockaddr_storage& addr, std::function<void(int)> cb);

    std::vector<PriorityTask> harvest_completed_tasks();

private:
    void select_loop();
    timeval next_timeout();
    void do_accept(const SelectTask& task);
    void do_read(const SelectTask& task);
    void do_write(const SelectTask& task);
    void on_connected(const SelectTask& task);
    void on_io_event(const std::map<int, SelectTask>& tasks, const fd_set& fds);
    std::tuple<std::map<int, SelectTask>, std::map<int, SelectTask>> get_pending_io();
    static void prepare_fd_set(const std::map<int, SelectTask>& tasks, fd_set& fds);

private:
    Event stop_event_;
    std::mutex mtx_;
    std::thread harvest_thread_;
    int max_rfd_ { 0 };
    int max_wfd_ { 0 };
    std::map<int, SelectTask> pending_rfds_;
    std::map<int, SelectTask> pending_wfds_;
    std::vector<PriorityTask> completed_task_;
    fd_set rfds_;
    fd_set wfds_;
    fd_set efds_;
};

} // namespace net

} // namespace bco