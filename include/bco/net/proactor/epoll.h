#pragma once
#include <netinet/in.h>
#include <sys/epoll.h>

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <thread>
#include <vector>

#include <bco/proactor.h>
#include <bco/executor.h>
#include <bco/net/address.h>
#include <bco/buffer.h>

namespace bco {

namespace net {

class Epoll {
    enum class Action : uint32_t {
        None = 0,
        Recv = 0b00001,
        Send = 0b00010,
        Recvfrom = 0b00100,
        Accept = 0b01000,
        Connect = 0b10000,
    };
    friend Action operator|(const Action& lhs, const Action& rhs);
    friend Action operator&(const Action& lhs, const Action& rhs);
    friend Action& operator|=(Action& lhs, const Action& rhs);
    friend Action& operator&=(Action& lhs, const Action& rhs);
    struct EpollItem {
        bco::Buffer buff;
        std::function<void(int)> cb;
        std::function<void(int, const sockaddr_storage&)> cb2;
    };
    struct EpollTask {
        epoll_event event;
        Action action;
        std::optional<EpollItem> read;
        std::optional<EpollItem> write;
    };

public:
    class GetterSetter {
    public:
        Epoll* proactor() { return epoll_.get(); }
        Epoll* socket_proactor() { return epoll_.get(); }
        void set_socket_proactor(std::unique_ptr<Epoll>&& ep)
        {
            epoll_ = std::move(ep);
        }

    private:
        std::unique_ptr<Epoll> epoll_;
    };

public:
    Epoll();
    ~Epoll();

    void start(ExecutorInterface* executor);
    void stop();

    int create(int domain, int type);
    int bind(int s, const sockaddr_storage& addr);
    int listen(int s, int backlog);

    int recv(int s,bco::Buffer buff, std::function<void(int)> cb);

    int recvfrom(int s,bco::Buffer buff, std::function<void(int, const sockaddr_storage&)> cb);

    int send(int s,bco::Buffer buff, std::function<void(int)> cb);
    int send(int s,bco::Buffer buff);

    int sendto(int s,bco::Buffer buff, const sockaddr_storage& addr);

    int accept(int listen_fd, std::function<void(int s)> cb);

    int connect(int s, const sockaddr_storage& addr, std::function<void(int)> cb);
    int connect(int s, const sockaddr_storage& addr);

    std::vector<PriorityTask> harvest_completed_tasks();

private:
    std::map<int, EpollTask> get_pending_tasks();
    void submit_tasks(std::map<int, EpollTask>& pending_tasks);
    void do_io();
    int send_sync(int s,bco::Buffer buff, std::function<void(int)> cb);
    int send_async(int s,bco::Buffer buff, std::function<void(int)> cb);
    void epoll_loop();
    int next_timeout();
    void on_io_event(const epoll_event& event);
    void do_recv(EpollTask& task);
    void do_recvfrom(EpollTask& task);
    void do_accept(EpollTask& task);
    void do_send(EpollTask& task);
    void on_connected(EpollTask& task);

private:
    std::mutex mtx_;
    int epoll_fd_;
    int exit_fd_;
    std::map<int, EpollTask> pending_tasks_;
    std::map<int, EpollTask> flying_tasks_;
    std::vector<PriorityTask> completed_task_;
    ExecutorInterface* io_executor_;
};

inline Epoll::Action operator|(const Epoll::Action& lhs, const Epoll::Action& rhs)
{
    return static_cast<Epoll::Action>(
                static_cast<std::underlying_type<Epoll::Action>::type>(lhs)
                | static_cast<std::underlying_type<Epoll::Action>::type>(rhs));
}

inline Epoll::Action operator&(const Epoll::Action& lhs, const Epoll::Action& rhs)
{
    return static_cast<Epoll::Action>(
                static_cast<std::underlying_type<Epoll::Action>::type>(lhs)
                & static_cast<std::underlying_type<Epoll::Action>::type>(rhs));
}

inline Epoll::Action& operator|=(Epoll::Action& lhs, const Epoll::Action& rhs)
{
    return lhs = lhs | rhs;
}

inline Epoll::Action& operator&=(Epoll::Action& lhs, const Epoll::Action& rhs)
{
    return lhs = lhs & rhs;
}

} // namespace net

} // namespace bco
