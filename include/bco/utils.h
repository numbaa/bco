#pragma once
#ifdef _WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#else
#include <fcntl.h>
#endif
#include <thread>
#include <mutex>


namespace bco {

inline void set_non_block(int fd)
{
    if (fd < 0)
        return;
#ifdef _WIN32
    u_long enable = 1;
    ::ioctlsocket(fd, FIONBIO, &enable);
#else
    int flags = ::fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#endif
}

/*
template <typename Function, typename... Args>
inline auto call_with_lock(Function&& func, Args&&... args) -> decltype(func(std::forward<Args>(args)...))
{
    std::lock_guard<std::mutex> lock(mtx);
    return func(args...);
}
*/

inline bool is_current_thread(const std::thread& th)
{
    return std::this_thread::get_id() == th.get_id();
}

inline bool should_try_again()
{
    return errno == EAGAIN || errno == EWOULDBLOCK;
}

inline in_addr to_ipv4(const std::string& str)
{
    in_addr addr {};
    inet_pton(AF_INET, str.c_str(), &addr);
    return addr;
}

inline in6_addr to_ipv6(const std::string& str)
{
    in6_addr addr {};
    inet_pton(AF_INET6, str.c_str(), &addr);
    return addr;
}

constexpr bool is_windows()
{
#ifdef _WIN32
    return true;
#else
    return false;
#endif
}

constexpr bool is_not_windows()
{
    return !is_windows();
}

inline int last_error()
{
#ifdef _WIN32
    return GetLastError();
#else
    return errno;
#endif
}

class WaitGroup {
public:
    explicit WaitGroup(uint32_t size)
        : size_(size)
    {
        if (size_ == 0)
            throw std::logic_error { "'size' smaller than zero" };
    }
    void done()
    {
        int size;
        {
            std::lock_guard lock { mtx_ };
            size_ -= 1;
            size = size;
        }
        if (size == 0)
            cv_.notify_one();
    }
    void wait()
    {
        std::unique_lock lock { mtx_ };
        cv_.wait(lock, [this]() { return size_ == 0; });
    }

private:
    std::mutex mtx_;
    std::condition_variable cv_;
    uint32_t size_;
};

namespace detail {
class ContextBase;
}

std::weak_ptr<detail::ContextBase> get_current_context();

}