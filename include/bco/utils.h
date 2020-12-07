#pragma once
#ifdef _WIN32
#include <WinSock2.h>
#else
//#include <>
#endif


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

}