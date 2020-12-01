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

class RootTask : public std::suspend_never {
public:
    class promise_type {
    public:
        RootTask get_return_object()
        {
            return RootTask {};
        }
        std::suspend_never initial_suspend()
        {
            return {};
        }
        std::suspend_never final_suspend()
        {
            return {};
        }
        void unhandled_exception()
        {
        }
        void return_void()
        {
        }
    };
};

template <typename T>
class ProactorTask {
    struct SharedContext {
        std::optional<T> result_;
        std::coroutine_handle<> caller_coroutine_;
    };

public:
    ProactorTask()
        : ctx_(new SharedContext)
    {
    }
    void set_result(T&& val) noexcept { ctx_->result_ = std::move(val); }
    bool await_ready() const noexcept { return ctx_->result_.has_value(); }
    void await_suspend(std::coroutine_handle<> coroutine) noexcept
    {
        ctx_->caller_coroutine_ = coroutine;
    }
    T await_resume() noexcept
    {
        //return std::move(result_.value_or(detail::default_value<T>());
        //return ctx_->result_.value_or(detail::default_value<T>());
        return ctx_->result_.value_or(T {});
    }
    void resume() { ctx_->caller_coroutine_.resume(); }

private:
    std::shared_ptr<SharedContext> ctx_;
};

template <>
class ProactorTask<void> {
    struct SharedContext {
        bool done_ = false;
        std::coroutine_handle<> caller_coroutine_;
    };

public:
    ProactorTask()
        : ctx_(new SharedContext)
    {
    }
    void set_done(bool done) noexcept { ctx_->done_ = done; }
    bool await_ready() const noexcept { return ctx_->done_; }
    std::function<void()> continuation()
    {
        return [this]() { ctx_->caller_coroutine_(); };
    }
    void await_suspend(std::coroutine_handle<> coroutine) noexcept
    {
        ctx_->caller_coroutine_ = coroutine;
    }
    void await_resume() noexcept { }
    void resume() { ctx_->caller_coroutine_.resume(); }

private:
    std::shared_ptr<SharedContext> ctx_;
};


} //namespace bco