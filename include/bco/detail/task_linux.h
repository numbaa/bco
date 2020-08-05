#pragma once
#include <coroutine>
#include <functional>

#include "promise_linux.h"

namespace bco {

template <typename T>
class Task {
public:
    using promise_type = promise_simple<Task<T>>;
    bool await_ready() const { return false; }
    T await_resume() { return result_; }
    void await_suspend(std::coroutine_handle<> handle)
    {
        co_task_(handle);
    }
    void set_result(T result)
    {
        result_ = result;
    }
    void set_co_task(std::function<void(std::coroutine_handle<>)> task)
    {
        co_task_ = task;
    }

private:
    T result_;
    std::function<void(std::coroutine_handle<>)> co_task_;
};

template <>
class Task<void> {
public:
    using promise_type = promise_simple<Task<void>>;
    bool await_ready() const { return false; }
    void await_resume() {}
    void await_suspend(std::coroutine_handle<> handle)
    {
        co_task_(handle);
    }
    void set_co_task(std::function<void(std::coroutine_handle<>)> task)
    {
        co_task_ = task;
    }

private:
    std::function<void(std::coroutine_handle<>)> co_task_;
};

} //namespace bco