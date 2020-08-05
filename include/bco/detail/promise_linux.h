#pragma once
#include <coroutine>
#include <exception>

namespace bco {

template <typename T>
class promise_simple {
public:
    auto get_return_object() -> T
    {
        return {};
    }
    auto initial_suspend() -> std::suspend_never
    {
        return {};
    }
    auto final_suspend() -> std::suspend_never
    {
        return {};
    }
    void unhandled_exception()
    {
        std::terminate();
    }
};

template <>
class promise_simple<void> {
public:
    auto initial_suspend() -> std::suspend_never
    {
        return {};
    }
    auto final_suspend() -> std::suspend_never
    {
        return {};
    }
    void unhandled_exception()
    {
        std::terminate();
    }
    void return_void() {}
};

} //namespace bco