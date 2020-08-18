#pragma once

#include <experimental/coroutine>
#include <functional>
#include <memory>
#include "promise_windows.h"

namespace bco {

template <typename PromiseType>
using coroutine_handle = std::experimental::coroutine_handle<PromiseType>;

using suspend_always = std::experimental::suspend_always;

using suspend_never = std::experimental::suspend_never;



namespace detail {

template <typename PromiseType>
class Awaitable {
public:
    Awaitable(coroutine_handle<PromiseType> coroutine)
        : coroutine_(coroutine)
    {
    }
    bool await_ready() const noexcept
    {
        return coroutine_.done();
    }
    coroutine_handle<> await_suspend(coroutine_handle<> coroutine) noexcept
    {
        coroutine_.promise().set_caller_coroutine(coroutine);
        return coroutine_;
    }
    decltype(auto) await_resume() noexcept
    {
        return coroutine_.promise().result();
    }

private:
    coroutine_handle<PromiseType> coroutine_;
};

template <typename TaskType>
class TaskPromise {
    struct FinalAwaitable {
        bool await_ready() noexcept { return false; }
        template <typename PromiseType>
        coroutine_handle<> await_suspend(coroutine_handle<PromiseType> coroutine) noexcept
        {
            return coroutine.promise().caller_coroutine();
        }
        void await_resume() noexcept { }
    };

public:
    TaskPromise() noexcept  = default;
    auto initial_suspend() noexcept
    {
        return suspend_awalys {};
    }
    auto final_suspend() noexcept
    {
        return FinalAwaitable {};
    }
    TaskType get_return_object() noexcept
    {
        return TaskType { coroutine_handle<TaskPromise>::frome_promise(*this) };
    }
    void unhandled_exception() noexcept;
    void caller_coroutine(coroutine_handle<> coroutine) noexcept
    {
        caller_ = coroutine;
    }
    coroutine_handle<> caller_coroutine() noexcept
    {
        return caller_;
    }

private:
    coroutine_handle<> caller_;
};

} //namespace detail

template <typename T = void>
class Task {
public:
    using promise_type = TaskPromise<Task<T>>;

    Task(coroutine_handle<promise_type> coroutine)
        : coroutine_(coroutine)
    {
    }

    auto operator co_await() const noexcept
    {
        return awaitable {coroutine_};
    }

private:
    coroutine_handle<promise_simple> coroutine_;
};

} //namespace bco