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

namespace bco {

namespace net {

class Epoll {
    enum class Action : uint32_t {
        None = 0,
        Read = 0b00001,
        Write = 0b00010,
        Accept = 0b00100,
        Connect = 0b01000,
    };
    friend Action operator|(const Action& lhs, const Action& rhs);
    friend Action operator&(const Action& lhs, const Action& rhs);
    friend Action& operator|=(Action& lhs, const Action& rhs);
    friend Action& operator&=(Action& lhs, const Action& rhs);
    struct EpollItem {
        std::span<std::byte> buff;
        std::function<void(int)> cb;
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
    int create_fd();
    void start();
    void stop();
    int read(int s, std::span<std::byte> buff, std::function<void(int length)> cb);
    int write(int s, std::span<std::byte> buff, std::function<void(int length)> cb);
    int accept(int s, std::function<void(int s)> cb);
    bool connect(int s, sockaddr_in addr, std::function<void(int)> cb);
    std::vector<PriorityTask> harvest_completed_tasks();

private:
    std::map<int, EpollTask> get_pending_tasks();
    void submit_tasks(const std::map<int, EpollTask>& pending_tasks);
    void epoll_loop();
    int next_timeout();
    void on_io_event(const epoll_event& event);
    void do_read(EpollTask& task);
    void do_accept(EpollTask& task);
    void do_write(EpollTask& task);
    void on_connected(EpollTask& task);

private:
    std::mutex mtx_;
    std::thread harvest_thread_;
    int epoll_fd_;
    int exit_fd_;
    std::map<int, EpollTask> pending_tasks_;
    std::map<int, EpollTask> flying_tasks_;
    std::vector<PriorityTask> completed_task_;
};

Epoll::Action operator|(const Epoll::Action& lhs, const Epoll::Action& rhs)
{
    return static_cast<Epoll::Action>(
                static_cast<std::underlying_type<Epoll::Action>::type>(lhs)
                | static_cast<std::underlying_type<Epoll::Action>::type>(rhs));
}

Epoll::Action operator&(const Epoll::Action& lhs, const Epoll::Action& rhs)
{
    return static_cast<Epoll::Action>(
                static_cast<std::underlying_type<Epoll::Action>::type>(lhs)
                & static_cast<std::underlying_type<Epoll::Action>::type>(rhs));
}

Epoll::Action& operator|=(Epoll::Action& lhs, const Epoll::Action& rhs)
{
    return lhs = lhs | rhs;
}

Epoll::Action& operator&=(Epoll::Action& lhs, const Epoll::Action& rhs)
{
    return lhs = lhs & rhs;
}

} // namespace net

} // namespace bco
