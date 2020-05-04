#pragma once
#include <experimental/coroutine>

namespace bco {

class promise_simple {
public:
    auto get_return_object() -> Task
    {
        return {};
    }
    auto initial_suspend() -> std::experimental::suspend_never
    {
        return {};
    }
    auto final_suspend() -> std::experimental::suspend_never
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