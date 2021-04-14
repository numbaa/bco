#pragma once
#include <chrono>
#include <functional>
#include <any>
#include <optional>
#include <type_traits>
#include <atomic>
#include <memory>
#include <concepts>

#include "task.h"
#include "bco/executor.h"

namespace bco {

namespace detail {

class SwitchTask : public Task<> {
public:
    SwitchTask(ExecutorInterface* executor);
    void await_suspend(std::coroutine_handle<> coroutine) noexcept;
private:
    ExecutorInterface* executor_;
};

class DelayTask : public Task<> {
public:
    DelayTask(std::chrono::milliseconds duration);
    void await_suspend(std::coroutine_handle<> coroutine) noexcept;
private:
    std::chrono::milliseconds duration_;
};

template <typename T>
class ExpirableTask : public Task<std::optional<T>> {
public:
    ExpirableTask(std::chrono::milliseconds duration, Task<T> task);
    void await_suspend(std::coroutine_handle<> coroutine) noexcept;

private:
    std::chrono::milliseconds duration_;
    Task<T> task_;
    std::shared_ptr<std::atomic<bool>> done_;
};

struct DoneFlag {
    bool done;
};

//TODO: 返回类型是void
template <typename Callable>
class ExpirableTaskAnyfunc : public Task<std::optional<std::invoke_result<Callable>>> {
public:
    ExpirableTaskAnyfunc(std::chrono::milliseconds duration, Callable&& callable);
    void await_suspend(std::coroutine_handle<> coroutine) noexcept;

private:
    std::chrono::milliseconds duration_;
    Callable func_;
    std::shared_ptr<std::atomic<bool>> done_;
};

template <typename Callable>
class ExecutorTask : public Task<std::invoke_result<Callable>> {
public:
    ExecutorTask(
        ExecutorInterface* current_executor,
        ExecutorInterface* target_executor,
        Callable&& func)
        : initial_executor_(current_executor)
        , target_executor_(target_executor)
        , func_(func)
    {
    }

    void await_suspend(std::coroutine_handle<> coroutine) noexcept
    {
        this->ctx_->caller_coroutine_ = coroutine;
        target_executor_->post(PriorityTask {
            1,
            [this, coroutine]() mutable {
                this->set_result(func_());
                this->initial_executor_->post(PriorityTask {
                    1,
                    std::bind(&ExecutorTask::resume, this) });
            } });
    }

private:
    ExecutorInterface* initial_executor_;
    ExecutorInterface* target_executor_;
    Callable func_;
};

} // namespace detail

class Timeout {
public:
    template <typename Rep, typename Period>
    explicit Timeout(std::chrono::duration<Rep, Period> duration)
        : Timeout { std::chrono::duration_cast<std::chrono::milliseconds>(duration) }
    {
    }
    template <>
    explicit Timeout(std::chrono::milliseconds duration)
        : duration_ { duration }
    {
    }
    const std::chrono::milliseconds duration() const
    {
        return duration_;
    }

private:
    std::chrono::milliseconds duration_;
};

template <typename Rep, typename Period>
[[nodiscard]] detail::DelayTask sleep_for(std::chrono::duration<Rep, Period> duration)
{
    return sleep_for(std::chrono::duration_cast<std::chrono::milliseconds>(duration));
}

template <>
[[nodiscard]] detail::DelayTask sleep_for(std::chrono::milliseconds duration);

[[nodiscard]] detail::SwitchTask switch_to(ExecutorInterface* executor);

template <typename Callable>
[[nodiscard]] detail::DelayTask run_with(ExecutorInterface* executor, Callable&& func)
{
    return detail::ExecutorTask { get_current_executor(), executor, func };
}

template <typename T>
[[nodiscard]] detail::ExpirableTask<std::optional<T>> run_with(Timeout timeout, Task<T> task)
{
    return detail::ExpirableTask { timeout.duration(), task };
}

template <typename Callable> requires std::invocable<Callable>
[[nodiscard]] detail::ExpirableTaskAnyfunc<std::optional<std::invoke_result<Callable>>> run_with(Timeout timeout, Callable&& func)
{
    return detail::ExpirableTaskAnyfunc { timeout.duration(), std::move(func) };
}


} // namespace bco
