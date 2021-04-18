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

inline int listen_socket(int s, int backlog)
{
    return ::listen(s, backlog);
}

inline int bind_socket(int s, const sockaddr_storage& addr)
{
    return ::bind(s, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
}

inline int close_socket(int s)
{
#ifdef _WIN32
    return ::closesocket(s);
#else
    return ::close(s);
#endif // _WIN32
}

inline int shutdown_socket(int s, int how)
{
    int _how;
#ifdef _WIN32
    switch (how) {
    case 0:
        _how = SD_RECEIVE;
        break;
    case 1:
        _how = SD_SEND;
        break;
    case 2:
    default:
        _how = SD_BOTH;
        break;
    }
#else
    switch (how) {
    case 0:
        _how = SHUT_RD;
        break;
    case 1:
        _how = SHUT_WR;
        break;
    case 2:
    default:
        _how = SD_RDWR;
        break;
    }
#endif
    return ::shutdown(s, _how);
}

#ifdef _WIN32
inline void* get_recvmsg_func(int s)
{
    GUID WSARecvMsg_GUID = WSAID_WSARECVMSG;
    LPFN_WSARECVMSG WSARecvMsg;
    DWORD NumberOfBytes;
    int ret = ::WSAIoctl(s, SIO_GET_EXTENSION_FUNCTION_POINTER,
                    &WSARecvMsg_GUID, sizeof(WSARecvMsg_GUID),
                    &WSARecvMsg, sizeof(WSARecvMsg),
                    &NumberOfBytes, NULL, NULL);
    if (ret == SOCKET_ERROR) {
        return nullptr;
    }
    return WSARecvMsg;
}
#endif // _WIN32

int syscall_sendv(int s, bco::Buffer buff);

int syscall_recvv(int s, bco::Buffer buff);

int syscall_sendmsg(int s, bco::Buffer buff, const sockaddr_storage& addr);

int syscall_recvmsg(int s, bco::Buffer buff, sockaddr_storage& addr, void* func_addr = nullptr);

} // namespace bco
