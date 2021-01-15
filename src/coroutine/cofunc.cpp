#include <bco/coroutine/cofunc.h>

namespace bco {

namespace detail {

SwitchTask::SwitchTask(ExecutorInterface* executor)
    : executor_(executor)
{
}
void SwitchTask::await_suspend(std::coroutine_handle<> coroutine) noexcept
{
    ctx_->caller_coroutine_ = coroutine;
    executor_->post(PriorityTask {
        1,
        std::bind(&SwitchTask::resume, this) });
}


DelayTask::DelayTask(ExecutorInterface* executor, std::chrono::microseconds duration)
        : executor_(executor)
        , duration_(duration)
    {
    }
    void DelayTask::await_suspend(std::coroutine_handle<> coroutine) noexcept
    {
        ctx_->caller_coroutine_ = coroutine;
        executor_->post_delay(
            std::chrono::duration_cast<std::chrono::microseconds>(duration_),
            PriorityTask {
                1,
                std::bind(&DelayTask::resume, this) });
    }

}

detail::SwitchTask switch_to(ExecutorInterface* executor)
{
    return detail::SwitchTask { executor };
}

template <>
detail::DelayTask sleep_for(std::chrono::microseconds duration)
{
    return detail::DelayTask {get_current_executor(), duration};
}

}