#pragma once
#ifdef _WIN32
#include <experimental/coroutine>
#else
#include <coroutine>
#endif // _WIN32

#include <experimental/coroutine>
#include <functional>
#include <memory>
#include <optional>

namespace bco {

#ifdef _WIN32
template <typename PromiseType>
using coroutine_handle = std::experimental::coroutine_handle<PromiseType>;

using suspend_always = std::experimental::suspend_always;

using suspend_never = std::experimental::suspend_never;
#else //_WIN32
template <typename PromiseType>
using coroutine_handle = std::coroutine_handle<PromiseType>;

using suspend_always = std::suspend_always;

using suspend_never = std::suspend_never;
#endif //_WIN32

namespace detail {

    template <typename _PromiseT>
    class Awaitable {
    public:
        Awaitable(coroutine_handle<_PromiseT> coroutine)
            : coroutine_(coroutine)
        {
        }
        bool await_ready() const noexcept
        {
            //Should I always return false?
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
        coroutine_handle<_PromiseT> coroutine_;
    };

    template <typename _TaskT>
    class TaskPromise {
        struct FinalAwaitable {
            bool await_ready() noexcept { return false; }
            template <typename _PromiseT>
            coroutine_handle<> await_suspend(coroutine_handle<_PromiseT> coroutine) noexcept
            {
                return coroutine.promise().caller_coroutine();
            }
            void await_resume() noexcept { }
        };

    public:
        TaskPromise() noexcept = default;
        auto initial_suspend() noexcept
        {
            return suspend_awalys {};
        }
        auto final_suspend() noexcept
        {
            return FinalAwaitable {};
        }
        _TaskT get_return_object() noexcept
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

    template <typename _TaskT>
    class GeneratorPromise {
    public:
        /*return some awaitable*/
        auto yield_value(_TaskT::value_type);
    };

} //namespace detail

template <typename T = void>
class Task {
public:
    using promise_type = detail::TaskPromise<Task<T>>;

    Task(coroutine_handle<promise_type> coroutine)
        : coroutine_(coroutine)
    {
    }

    auto operator co_await() const noexcept
    {
        return detail::Awaitable { coroutine_ };
    }

private:
    coroutine_handle<promise_type> coroutine_;
};

template <typename T>
class Generator {
public:
    using promise_type = detail::GeneratorPromise<Generator<T>>;
    using value_type = T;
    
    Generator(coroutine_handle<promise_type> coroutine)
        : coroutine_(coroutine)
    {
    }

    auto operator co_await() const noexcept
    {
        return detail::Awaitable { coroutine_ };
    }

private:
    coroutine_handle<promise_type> coroutine_;
};

//TODO: find another name more general
template <typename T>
class IoTask {
public:
    IoTask() = default;
    void set_result(T&& val) noexcept { result_ = std::move(val); }
    bool await_ready() const noexcept { return result_.has_value(); }
    //this is caller's coroutine, what type it is?
    coroutine_handle<> await_suspend(coroutine_handle<> coroutine) noexcept
    {
        caller_coroutine_ = coroutine;
    }
    T&& await_resume() noexcept
    {
        return std::move(result_.value_or(detail::default_value<T>());
    }
    void resume() { caller_coroutine_.resume(); }
private:
    std::optional<T> result_;
    coroutine_handle<> caller_coroutine_;
};

template <>
class IoTask<void> {
public:
    IoTask() = default;
    void set_done(bool done) noexcept { done_ = done; }
    bool await_ready() const noexcept { return done_; }
    std::function<void()> continuation()
    {
        return [this]() { caller_coroutine_(); };
    }
    //this is caller's coroutine, what type it is?
    void await_suspend(coroutine_handle<void> coroutine) noexcept
    {
        caller_coroutine_ = coroutine;
    }
    void await_resume() noexcept {}
    void resume() { caller_coroutine_.resume(); }

private:
    bool done_ = false;
    coroutine_handle<void> caller_coroutine_;
};


} //namespace bco