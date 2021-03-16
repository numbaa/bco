#pragma once
#ifdef _WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <MSWSock.h>
#else
#include <fcntl.h>
#endif
#include <thread>
#include <mutex>

#include <bco/buffer.h>


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
            size = size_;
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

int syscall_sendv(int s, bco::Buffer buff)
{
    auto slices = buff.cdata();
#ifdef _WIN32
    std::vector<WSABUF> wsabuf(slices.size());
    for (size_t i = 0; i < wsabuf.size(); i++) {
        wsabuf[i].buf = reinterpret_cast<CHAR*>(slices[i].data());
        wsabuf[i].len = static_cast<ULONG>(slices[i].size());
    }
    DWORD bytes_sent = 0;
    int ret = ::WSASend(s, wsabuf.data(), static_cast<DWORD>(wsabuf.size()), &bytes_sent, 0, nullptr, nullptr);
    if (ret != 0) {
        return SOCKET_ERROR;
    } else {
        return bytes_sent;
    }
#else
    std::vector<::iovec> iovecs(slices.size());
    for (size_t i = 0; i < iovecs.size(); i++) {
        iovecs[i] = ::iovec { slices[i].data(), slices[i].size() };
    }
    return ::writev(task.fd, &iovecs, iovecs.size());
#endif // _WIN32
}

int syscall_recvv(int s, bco::Buffer buff)
{
    auto slices = buff.data();
#ifdef _WIN32
    std::vector<WSABUF> wsabuf(slices.size());
    for (size_t i = 0; i < wsabuf.size(); i++) {
        wsabuf[i].buf = reinterpret_cast<CHAR*>(slices[i].data());
        wsabuf[i].len = static_cast<ULONG>(slices[i].size());
    }
    DWORD bytes_read = 0;
    DWORD flag = 0;
    int ret = ::WSARecv(s, wsabuf.data(), static_cast<DWORD>(wsabuf.size()), &bytes_read, &flag, nullptr, nullptr);
    if (ret != 0) {
        return SOCKET_ERROR;
    } else {
        return bytes_read;
    }
#else
    std::vector<::iovec> iovecs(slices.size());
    for (size_t i = 0; i < iovecs.size(); i++) {
        iovecs[i] = ::iovec { slices[i].data(), slices[i].size() };
    }
    return ::readv(task.fd, &iovecs, iovecs.size());
#endif // _WIN32
}

int syscall_sendmsg(int s, bco::Buffer buff, const sockaddr_storage& addr)
{
    auto slices = buff.cdata();
#ifdef _WIN32
    std::vector<WSABUF> wsabuf(slices.size());
    for (size_t i = 0; i < wsabuf.size(); i++) {
        wsabuf[i].buf = reinterpret_cast<CHAR*>(slices[i].data());
        wsabuf[i].len = static_cast<ULONG>(slices[i].size());
    }
    WSAMSG wsamsg
    {
        .name = const_cast<sockaddr*>(reinterpret_cast<const sockaddr*>(&addr)),
        .namelen = sizeof(addr),
        .lpBuffers = wsabuf.data(),
        .dwBufferCount = static_cast<ULONG>(wsabuf.size()),
        .Control = WSABUF {},
        .dwFlags = 0,
    };
    DWORD bytes_sent = 0;
    int ret = ::WSASendMsg(s, &wsamsg, 0, &bytes_sent, nullptr, nullptr);
    if (ret != 0) {
        return SOCKET_ERROR;
    } else {
        return bytes_sent;
    }
#else
    std::vector<::iovec> iovecs(slices.size());
    for (size_t i = 0; i < iovecs.size(); i++) {
        iovecs[i] = ::iovec { slices[i].data(), slices[i].size() };
    }
    ::msghdr hdr {
        .msg_name = const_cast<sockaddr*>(reinterpret_cast<const sockaddr*>(&addr)),
        .msg_namelen = sizeof(addr),
        .msg_iov = iovecs.data(),
        .msg_iovlen = iovecs.size(),
        .msg_control = nullptr,
        .msg_controllen = 0,
        .msg_flags = 0,
    };
    return ::sendmsg(s, &hdr, 0);
#endif // _WIN32
}

int syscall_recvmsg(int s, bco::Buffer buff, sockaddr_storage& addr)
{
    auto slices = buff.data();
#ifdef _WIN32
    std::vector<WSABUF> wsabuf(slices.size());
    for (size_t i = 0; i < wsabuf.size(); i++) {
        wsabuf[i].buf = reinterpret_cast<CHAR*>(slices[i].data());
        wsabuf[i].len = static_cast<ULONG>(slices[i].size());
    }
    WSAMSG wsamsg {
        .name = reinterpret_cast<sockaddr*>(&addr),
        .namelen = sizeof(addr),
        .lpBuffers = wsabuf.data(),
        .dwBufferCount = static_cast<ULONG>(wsabuf.size()),
        .Control = WSABUF {},
        .dwFlags = 0,
    };
    DWORD bytes_read = 0;
    LPFN_WSARECVMSG WSARecvMsg = nullptr; //TODO: get function ptr
    int ret = WSARecvMsg(s, &wsamsg, &bytes_read, nullptr, nullptr);
    if (ret != 0) {
        return SOCKET_ERROR;
    } else {
        return bytes_read;
    }
#else
    std::vector<::iovec> iovecs(slices.size());
    for (size_t i = 0; i < iovecs.size(); i++) {
        iovecs[i] = ::iovec { slices[i].data(), slices[i].size() };
    }
    ::msghdr hdr {
        .msg_name = const_cast<sockaddr*>(reinterpret_cast<const sockaddr*>(&addr)),
        .msg_namelen = sizeof(addr),
        .msg_iov = iovecs.data(),
        .msg_iovlen = iovecs.size(),
        .msg_control = nullptr,
        .msg_controllen = 0,
        .msg_flags = 0,
    };
    return ::recvmsg(s, &hdr, 0);
#endif // _WIN32
}

}