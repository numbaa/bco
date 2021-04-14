#include <bco/coroutine/cofunc.h>
#include <bco/context.h>

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


DelayTask::DelayTask(std::chrono::milliseconds duration)
        : duration_(duration)
    {
    }
    void DelayTask::await_suspend(std::coroutine_handle<> coroutine) noexcept
    {
        ctx_->caller_coroutine_ = coroutine;
        get_current_executor()->post_delay(
            std::chrono::duration_cast<std::chrono::milliseconds>(duration_),
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
detail::DelayTask sleep_for(std::chrono::milliseconds duration)
{
    return detail::DelayTask {duration};
}


template <typename T>
detail::ExpirableTask<T>::ExpirableTask(std::chrono::milliseconds duration, Task<T> task)
    : duration_(duration)
    , task_(task)
    , done_(new std::atomic<bool> { false })
{
}

template <typename T>
void detail::ExpirableTask<T>::await_suspend(std::coroutine_handle<> coroutine) noexcept
{
    ctx_->caller_coroutine_ = coroutine;
    auto exe_ctx = get_current_context().lock();
    auto done = done_;
    exe_ctx->spawn([this, done]() -> Routine {
        bool _done = false;
        if (done->compare_exchange_strong(_done, true)) {
            this->set_result(std::optional<T>(co_await task));
            this->resume();
        }
    });
    get_current_executor()->post_delay(duration_, PriorityTask {
        .priority = 1,
        .task = [this, done]() {
            bool _done = false;
            if (done->compare_exchange_strong(_done, true)) {
                this->resume();
            }
        },
    });
}

template <typename Callable>
 detail::ExpirableTaskAnyfunc<Callable>::ExpirableTaskAnyfunc(std::chrono::milliseconds duration, Callable&& callable)
    : duration_(duration)
    , func_(std::move(callable))
    , done_(new std::atomic<bool> { false })
{
}

template <typename Callable>
void detail::ExpirableTaskAnyfunc<Callable>::await_suspend(std::coroutine_handle<> coroutine) noexcept
{
    ctx_->caller_coroutine_ = coroutine;
    auto done = done_;
    get_current_executor()->post(PriorityTask {
        .priority = 1,
        .task = [this, done]() {
            bool _done = false;
            if (done->compare_exchange_strong(_done, true)) {
                this->set_result(std::optional<std::invoke_result<Callable>>(func_()));
                this->resume();
            }
        },
    });
    get_current_executor()->post_delay(duration_, PriorityTask {
        .priority = 1,
        .task = [this, done]() {
            bool _done = false;
            if (done->compare_exchange_strong(_done, true)) {
                this->resume();
            }
        },
    });
}


} // namespace bco