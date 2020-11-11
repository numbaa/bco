#pragma once
#include <coroutine>
#include <functional>
#include <memory>
#include <optional>

#include <bco/detail/utils.inl>
#include <bco/detail/task.inl>

namespace bco {

template <typename T = void>
class Task;

template <typename T>
class Task {
public:
    using promise_type = detail::PromiseType<Task, T>;
    auto operator co_await()
    {
        return detail::Awaitable<promise_type> { coroutine_ };
    }

private:
    Task(std::coroutine_handle<promise_type> coroutine)
        : coroutine_(coroutine)
    {
    }
    std::coroutine_handle<promise_type> coroutine_;
    friend class promise_type;
};

template <typename T>
class ProactorTask {
public:
    ProactorTask() = default;
    void set_result(T&& val) noexcept { result_ = std::move(val); }
    bool await_ready() const noexcept { return result_.has_value(); }
    void await_suspend(std::coroutine_handle<> coroutine) noexcept
    {
        caller_coroutine_ = coroutine;
    }
    T await_resume() noexcept
    {
        //return std::move(result_.value_or(detail::default_value<T>());
        return result_.value_or(detail::default_value<T>());
    }
    void resume() { caller_coroutine_.resume(); }

private:
    std::optional<T> result_;
    std::coroutine_handle<> caller_coroutine_;
};

template <>
class ProactorTask<void> {
public:
    ProactorTask() = default;
    void set_done(bool done) noexcept { done_ = done; }
    bool await_ready() const noexcept { return done_; }
    std::function<void()> continuation()
    {
        return [this]() { caller_coroutine_(); };
    }
    void await_suspend(std::coroutine_handle<> coroutine) noexcept
    {
        caller_coroutine_ = coroutine;
    }
    void await_resume() noexcept { }
    void resume() { caller_coroutine_.resume(); }

private:
    bool done_ = false;
    std::coroutine_handle<> caller_coroutine_;
};


} //namespace bco