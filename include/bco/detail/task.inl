#pragma once

namespace bco {

namespace detail {

template <template <typename> typename _TaskT, typename T>
class PromiseType;

template <template <typename> typename _TaskT>
class PromiseType<_TaskT, void> {
    struct FinalAwaitable {
        bool await_ready() noexcept { return false; }
        //co_await await_suspend()是在当前coroutine执行，传进去的也是当前coroutine_handle
        template <typename _PromiseT>
        std::coroutine_handle<void> await_suspend(std::coroutine_handle<_PromiseT> coroutine) noexcept
        {
            return coroutine.promise().caller_coroutine();
        }
        void await_resume() noexcept { }
        void return_void() { }
    };

public:
    //构造函数不能有参数、必须public
    PromiseType() = default;
    _TaskT<void> get_return_object()
    {
        return _TaskT<void> { std::coroutine_handle<PromiseType>::from_promise(*this) };
    }
    //执行到coroutine的左大括号，就会执行co_await initial_suspend()
    //这里让它立即挂起，该coroutine的caller会执行co_await return_object
    //以便在coroutine开始执行前获取到外部的coroutine_handle
    std::suspend_always initial_suspend()
    {
        return {};
    }
    FinalAwaitable final_suspend()
    {
        return {};
    }
    std::coroutine_handle<> caller_coroutine()
    {
        return caller_coroutine_;
    }
    void unhandled_exception()
    {
    }
    void set_caller_coroutine(std::coroutine_handle<void> caller)
    {
        caller_coroutine_ = caller;
    }
    void return_void() { }
    void result() { }

private:
    std::coroutine_handle<> caller_coroutine_;
};

template <template <typename> typename _TaskT, typename T>
class PromiseType : PromiseType<_TaskT, void> {
public:
    _TaskT<T> get_return_object()
    {
        return _TaskT<void> { std::coroutine_handle<PromiseType>::from_promise(*this) };
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

template <typename _PromiseT>
class Awaitable {
public:
    Awaitable(std::coroutine_handle<_PromiseT> coroutine)
        : current_coroutine_(coroutine)
    {
    }
    bool await_ready()
    {
        return false;
    }
    decltype(auto) await_resume() noexcept
    {
        return current_coroutine_.promise().result();
    }
    //返回coroutine_handle<Task>似乎也行
    std::coroutine_handle<> await_suspend(std::coroutine_handle<> caller_coroutine)
    {
        current_coroutine_.promise().set_caller_coroutine(caller_coroutine);
        return current_coroutine_;
    }

private:
    std::coroutine_handle<_PromiseT> current_coroutine_;
};

} // namespace detail

} // namespace bco