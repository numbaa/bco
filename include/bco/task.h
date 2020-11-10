#pragma once
#ifdef _WIN32
#include <experimental/coroutine>
#else
#include <coroutine>
#endif // _WIN32

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

namespace detail
{

template <template <typename> typename _TaskT, typename T>
class PromiseType;

template <template <typename> typename _TaskT>
class PromiseType<_TaskT, void> {
    struct FinalAwaitable {
        bool await_ready() noexcept { return false; }
        //co_await await_suspend()是在当前coroutine执行，传进去的也是当前coroutine_handle
        template <typename _PromiseT>
        coroutine_handle<> await_suspend(coroutine_handle<_PromiseT> coroutine) noexcept
        {
            return coroutine.promise().caller_coroutine();
        }
        void await_resume() noexcept { }
        void return_void() {}
    };
public:
    //构造函数不能有参数、必须public
    PromiseType() = default;
    _TaskT<void> get_return_object()
    {
        return _TaskT<void>{coroutine_handle<PromiseType>::from_promise(*this)};
    }
    //执行到coroutine的左大括号，就会执行co_await initial_suspend()
    //这里让它立即挂起，该coroutine的caller会执行co_await return_object
    //以便在coroutine开始执行前获取到外部的coroutine_handle
    suspend_always initial_suspend()
    {
        return {};
    }
    FinalAwaitable final_suspend()
    {
        return {};
    }
    coroutine_handle<> caller_coroutine()
    {
        return caller_coroutine_;
    }
    void set_caller_coroutine(coroutine_handle<> caller)
    {
        caller_coroutine_ = caller;
    }
    void return_void() {}
    void result() {}


private:
    coroutine_handle<void> caller_coroutine_;
};

template <template <typename> typename _TaskT, typename T>
class PromiseType : PromiseType<_TaskT<void>> {
public:
    _TaskT<T> get_return_object()
    {
        return _TaskT<void>{coroutine_handle<PromiseType>::from_promise(*this)};
    }
    void return_value(T t)
    {
        value_ = t;
    }
    T result()
    {
        return value_;
    }
private:
    T value_;
};
    
} // namespace detail


template <typename T = void>
class Task {
public:
    using promise_type = detail::PromiseType<Task<T>>;
    bool await_ready()
    {
        return false;
    }
    decltype(auto) await_resume() noexcept
    {
        return current_coroutine_.promise().result();
    }
    //返回coroutine_handle<Task>似乎也行
    coroutine_handle<> await_suspend(coroutine_handle<> caller_coroutine)
    {
        current_coroutine_.promise().set_caller_coroutine(caller_coroutine);
        return current_coroutine_;
    }
private:
    Task(coroutine_handle<promise_type> current_coroutine)
    {
        current_coroutine_ = current_coroutine;
    }
    coroutine_handle<promise_type> current_coroutine_;
    friend class promise_type;
};

template <typename T>
class ProactorTask {
public:
    ProactorTask() = default;
    void set_result(T&& val) noexcept { result_ = std::move(val); }
    bool await_ready() const noexcept { return result_.has_value(); }
    //this is caller's coroutine, what type it is?
    coroutine_handle<void> await_suspend(coroutine_handle<void> coroutine) noexcept
    {
        caller_coroutine_ = coroutine;
    }
    T await_resume() noexcept
    {
        //return std::move(result_.value_or(detail::default_value<T>());
        return result_.value_or(detail::default_value<T>();
    }
    void resume() { caller_coroutine_.resume(); }
private:
    std::optional<T> result_;
    coroutine_handle<void> caller_coroutine_;
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