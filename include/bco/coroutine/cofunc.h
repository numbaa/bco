#pragma once
#include <chrono>
#include <functional>
#include <type_traits>
//#include <concepts>

#include "task.h"
#include "bco/executor.h"

namespace bco {

auto switch_to(ExecutorInterface* executor)
{
    class SwitchTask : public ProactorTask<> {
    public:
        SwitchTask(ExecutorInterface* executor)
            : executor_(executor)
        {
        }
        void await_suspend(std::coroutine_handle<> coroutine) noexcept
        {
            ctx_->caller_coroutine_ = coroutine;
            executor_->post(PriorityTask {
                1,
                std::bind(&SwitchTask::resume, this) });
        }

    private:
        ExecutorInterface* executor_;
    };
    return SwitchTask {executor};
}

template <typename Rep, typename Period>
auto sleep_for(std::chrono::duration<Rep, Period> duration)
{
    class DelayTask : public ProactorTask<> {
    public:
        DelayTask(ExecutorInterface* executor, std::chrono::duration<Rep, Period> duration)
            : executor_(executor)
            , duration_(duration)
        {
        }
        void await_suspend(std::coroutine_handle<> coroutine) noexcept
        {
            ctx_->caller_coroutine_ = coroutine;
            executor_->post_delay(
                std::chrono::duration_cast<std::chrono::microseconds>(duration_),
                PriorityTask {
                    1,
                    std::bind(&DelayTask::resume, this) });
        }

    private:
        ExecutorInterface* executor_;
        std::chrono::duration<Rep, Period> duration_;
    };
    return DelayTask { get_current_executor(), duration };
}

template <typename Callable>
auto run_with(ExecutorInterface* executor, Callable&& func)
{
    class ExecutorTask : public ProactorTask<std::invoke_result<Callable>> {
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
            ctx_->caller_coroutine_ = coroutine;
            target_executor_->post(PriorityTask {
                1,
                [this, coroutine]() mutable {
                    this->set_result(func_());
                    this->initial_executor_->post(PriorityTask {
                        1,
                        std::bind(&ExecutorTask::resume, this)
                    });
                }
            });
        }

    private:
        ExecutorInterface* initial_executor_;
        ExecutorInterface* target_executor_;
        Callable func_;
    };
    return ExecutorTask { get_current_executor(), executor, func };
}

}