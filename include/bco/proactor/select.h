#pragma once
#ifdef _WIN32
#include <WinSock2.h>
#else // _WIN32
#include <netinet/in.h>
#endif

#include <functional>
#include <memory>
#include <span>
#include <array>s
#include <vector>
#include <mutex>
#include <map>

namespace bco {

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
    Select();
    ~Select();
    void start();
    int read(int s, std::span<std::byte> buff, std::function<void(int length)>&& cb);

    int write(int s, std::span<std::byte> buff, std::function<void(int length)>&& cb);

    int accept(int s, std::function<void(int s)>&& cb);

    bool connect(int s, sockaddr_in addr, std::function<void(int)>&& cb);

    std::vector<std::function<void()>> drain(uint32_t timeout_ms);

    void attach(int fd) { }

private:
    void select_loop();
    timeval next_timeout();
    void do_accept(SelectTask task);
    void do_read(SelectTask task);
    void do_write(SelectTask task);
    void on_connected(SelectTask task);
    void on_io_event(const std::map<int, SelectTask>& x, const fd_set& fds);
    std::tuple<std::map<int, SelectTask>, std::map<int, SelectTask>> get_pending_io();
    static void prepare_fd_set(const std::map<int, SelectTask>& x, fd_set& fds);


private:
    std::mutex mtx_;
    std::thread harvest_thread_;
    int max_rfd_ { 0 };
    int max_wfd_ { 0 };
    std::map<int, SelectTask> pending_rfds_;
    std::map<int, SelectTask> pending_wfds_;
    std::vector<std::function<void()>> completed_task_;
    fd_set rfds_;
    fd_set wfds_;
    fd_set efds_;
};

}
