#pragma once

#ifdef _WIN32
// clang-format off
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <MSWSock.h>
// clang-format on
#else
#include <arpa/inet.h>
#include <fcntl.h>
#endif

#include <condition_variable>
#include <mutex>
#include <thread>

#include <bco/buffer.h>
#include <bco/executor.h>

namespace bco {

/*
template <typename Function, typename... Args>
inline auto call_with_lock(Function&& func, Args&&... args) -> decltype(func(std::forward<Args>(args)...))
{
    std::lock_guard<std::mutex> lock(mtx);
    return func(args...);
}
*/

namespace detail {

template <typename T>
inline void write_different_endian(uint8_t* buff, const T& value)
{
    for (size_t i = 0; i < sizeof(value); i++) {
        buff[i] = static_cast<uint8_t>(value >> ((sizeof(value) - 1 - i) * 8));
    }
}

template <typename T>
inline void read_different_endian(const uint8_t* buff, T& value)
{
    value = 0;
    for (size_t i = 0; i < sizeof(value); i++) {
        value |= buff[i] << ((sizeof(value) - i - 1) * 8);
    }
}

template <typename T>
inline void write_same_endian(uint8_t* buff, const T& value)
{
    for (size_t i = 0; i < sizeof(value); i++) {
        buff[i] = static_cast<uint8_t>(value >> (i * 8));
    }
}

template <typename T>
inline void read_same_endian(const uint8_t* buff, T& value)
{
    value = 0;
    for (size_t i = 0; i < sizeof(value); i++) {
        value |= buff[i] << (i * 8);
    }
}

} // namespace detial

template <typename T>
inline void write_big_endian(uint8_t* buff, const T& value)
{
    if constexpr (std::endian::native == std::endian::big) {
        detail::write_same_endian(buff, value);
    } else {
        detail::write_different_endian(buff, value);
    }
}

template <typename T>
inline void read_big_endian(const uint8_t* buff, T& value)
{
    if constexpr (std::endian::native == std::endian::big) {
        detail::read_same_endian(buff, value);
    } else {
        detail::read_different_endian(buff, value);
    }
}

template <typename T>
inline void write_little_endian(uint8_t* buff, const T& value)
{
    if constexpr (std::endian::native == std::endian::little) {
        detail::write_same_endian(buff, value);
    } else {
        detail::write_different_endian(buff, value);
    }
}

template <typename T>
inline void read_little_endian(const uint8_t* buff, T& value)
{
    if constexpr (std::endian::native == std::endian::little) {
        detail::read_same_endian(buff, value);
    } else {
        detail::read_different_endian(buff, value);
    }
}

inline in_addr to_in_addr(const std::string& str)
{
    in_addr addr {};
    inet_pton(AF_INET, str.c_str(), &addr);
    return addr;
}

inline in6_addr to_in6_addr(const std::string& str)
{
    in6_addr addr {};
    inet_pton(AF_INET6, str.c_str(), &addr);
    return addr;
}

class WaitGroup {
public:
    explicit WaitGroup(size_t size)
        : size_(size)
    {
        if (size_ == 0)
            throw std::logic_error { "WaitGroup: 'size' equal to zero" };
    }
    void done()
    {
        size_t size;
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
    size_t size_;
};

class Context;

void set_current_thread_context(std::weak_ptr<Context> ctx);
std::weak_ptr<Context> get_current_context();
ExecutorInterface* get_current_executor();

} // namespace bco
