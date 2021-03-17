#pragma once

#ifdef _WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <MSWSock.h>
#else
#include <fcntl.h>
#endif

#include <thread>

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

inline bool is_current_thread(const std::thread& th)
{
    return std::this_thread::get_id() == th.get_id();
}

inline bool should_try_again()
{
    return errno == EAGAIN || errno == EWOULDBLOCK;
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

int syscall_sendv(int s, bco::Buffer buff);

int syscall_recvv(int s, bco::Buffer buff);

int syscall_sendmsg(int s, bco::Buffer buff, const sockaddr_storage& addr);

int syscall_recvmsg(int s, bco::Buffer buff, sockaddr_storage& addr);

} // namespace bco
