#pragma once
#include <WinSock2.h>
#include <cstdint>
#include <concepts>
#include <span>
#include <functional>

namespace bco {

/*
template <typename T>
concept Buffer = requires(T b) {
    {
        b.data()
    }
    ->std::convertible_to<void*>;
    {
        b.size()
    }
    ->std::same_as<size_t>;
};
*/

template <typename T>
concept Readable = requires(T r, int s, std::span<std::byte> buff, std::function<void(int length)>&& cb)
{
    {
        r.read(s, buff, std::move(cb))
    }
    ->std::same_as<int>;
};

template <typename T>
concept Writable = requires(T r, int s, std::span<std::byte> buff, std::function<void(int length)>&& cb)
{
    {
        r.write(s, buff, std::move(cb))
    }
    ->std::same_as<int>;
};

template <typename T>
concept Acceptable = requires(T p, int s, std::function<void(int length)>&& cb) {
    {
        p.accept(s, std::move(cb))
    }
    ->std::same_as<int>;
};

template <typename T>
concept Connectable = requires(T p, sockaddr_in addr, std::function<void(int length)>&& cb)
{
    {
        p.connect(addr, std::move(cb))
    }
    ->std::same_as<bool>;
};

template <typename T>
concept Drainable = requires(T r, uint32_t timeout_ms)
{
    {
        r.drain(timeout_ms)
    }
    ->std::same_as<std::vector<std::function<void()>>>;
};

template <typename T>
concept Attachable = requires(T r, int s)
{
    {
        r.attach(s)
    }
    ->std::same_as<void>;
};

template <typename T>
concept Proactor = Readable<T>&& Writable<T>&& Acceptable<T>&& Connectable<T>&& Drainable<T>&& Attachable<T>;

}