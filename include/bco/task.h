#pragma once
#include <coroutine>

namespace bco {

template <typename T>
class Task {
public:
    using promise_type = promise_simple;
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
    void set_co_task(std::function<void()> task)
    {
        co_task_ = task;
    }

private:
    T result_;
    std::function<void(std::coroutine_handle<>)> co_task_;
};

} //namespace bco