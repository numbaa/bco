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
    struct ST {
        int fd;
        std::span<std::byte> buff;
        std::function<void(int length)> cb;
    };

public:
    Select();

    int read(int s, std::span<std::byte> buff, std::function<void(int length)>&& cb);

    int write(int s, std::span<std::byte> buff, std::function<void(int length)>&& cb);

    int accept(int s, std::function<void(int s)>&& cb);

    bool connect(sockaddr_in addr, std::function<void(int)>&& cb);

    std::vector<std::function<void()>> drain(uint32_t timeout_ms);

    void attach(int fd);

private:
    void select_loop();
    timeval next_timeout();
    void do_accept(std::function<void(int s)> cb);
    void do_read(std::function<void(int s)> cb);
    void do_write(std::function<void(int s)> cb);
    void do_connect(std::function<void(int s)> cb);
    void try_do_io(const std::map<int, ST>& x, const fd_set& fds);
    std::tuple<std::map<int, ST>, std::map<int, ST>> get_pending_io();
    static void prepare_fd_set(const std::map<int, ST>& x, fd_set& fds);


private:
    std::mutex mtx_;
    int max_rfd_;
    int max_wfd_;
    std::map<int, ST> pending_rfds_;
    std::map<int, ST> pending_wfds_;
    fd_set rfds_;
    fd_set wfds_;
    fd_set efds_;
};

}