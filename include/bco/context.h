#pragma once
#include <memory>
#include <functional>
#include <algorithm>
#include <bco/executor.h>
#include <bco/proactor.h>
#include <bco/coroutine/task.h>

namespace bco
{

namespace detail {

class ContextBase : public ::std::enable_shared_from_this<ContextBase> {
public:
    ContextBase() = default;
    void start()
    {
        executor_->set_context(weak_from_this());
        executor_->start();
    }

protected:
    std::unique_ptr<ExecutorInterface> executor_;
};

}

template <typename...Types>
class Context;

template <typename T, typename...Types> requires Proactor<T>
class Context<T, Types...> : public T::GetterSetter, public Context<Types...> {
public:
    std::vector<PriorityTask> get_proactor_tasks()
    {
        auto tasks = T::GetterSetter::proactor()->harvest_completed_tasks();
        std::ranges::copy(Context<Types...>::get_proactor_tasks(), std::back_inserter(tasks));
        return tasks;
    }
};


template <typename T> requires Proactor<T>
class Context<T> : public T::GetterSetter, public detail::ContextBase {
public:
    Context() = default;
    Context(std::unique_ptr<ExecutorInterface>&& executor)
        : executor_(std::move(executor))
    {
        executor_->set_proactor_task_getter(std::bind(&Context::get_proactor_tasks, this));
    }
    void spawn(std::function<Task<>()>&& coroutine)
    {
        executor_->post(PriorityTask { 0, std::bind(&Context::spawn_aux1, this, coroutine) });
    }
    void set_executor(std::unique_ptr<ExecutorInterface>&& executor)
    {
        executor_ = std::move(executor);
        executor_->set_proactor_task_getter(std::bind(&Context::get_proactor_tasks, this));
    }
    std::vector<PriorityTask> get_proactor_tasks()
    {
        return T::GetterSetter::proactor()->harvest_completed_tasks();
    }

private:
    void spawn_aux1(std::function<Task<>()> coroutine)
    {
        spawn_aux2(std::move(coroutine));
    }
    RootTask spawn_aux2(std::function<Task<>()>&& coroutine)
    {
        co_await coroutine();
    }


};

} // namespace bco
