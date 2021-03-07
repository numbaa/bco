#pragma once
#include <chrono>
#include <functional>
#include <any>
#include <type_traits>
//#include <concepts>

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
    DelayTask(ExecutorInterface* executor, std::chrono::microseconds duration);
    void await_suspend(std::coroutine_handle<> coroutine) noexcept;
private:
    ExecutorInterface* executor_;
    std::chrono::microseconds duration_;
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

}

template <typename Rep, typename Period>
[[nodiscard]] detail::DelayTask sleep_for(std::chrono::duration<Rep, Period> duration)
{
    return sleep_for(std::chrono::duration_cast<std::chrono::microseconds>(duration));
}

template <>
[[nodiscard]] detail::DelayTask sleep_for(std::chrono::microseconds duration);

[[nodiscard]] detail::SwitchTask switch_to(ExecutorInterface* executor);

template <typename Callable>
[[nodiscard]] detail::DelayTask run_with(ExecutorInterface* executor, Callable&& func)
{
    return detail::ExecutorTask { get_current_executor(), executor, func };
}

}