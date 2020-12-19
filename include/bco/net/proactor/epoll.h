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
        Read,
        Write,
        Accept,
        Connect,
    };
    struct EpollItem {
        std::span<std::byte> buff;
        std::function<void(int)> cb;
    };
    struct EpollTask {
        epoll_event event;
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
    void epoll_loop();
    int next_timeout();
    void on_io_event(const epoll_event& event);
    void do_read(const epoll_event& event);

private:
    std::mutex mtx_;
    std::thread harvest_thread_;
    int epoll_fd_;
    int exit_fd_;
    std::map<int, EpollTask> pending_tasks_;
    std::map<int, EpollTask> flying_tasks_;
    std::vector<PriorityTask> completed_task_;
};

} // namespace net

} // namespace bco
