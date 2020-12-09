#pragma once
#include <memory>
#include <functional>
#include <algorithm>
#include <bco/executor.h>
#include <bco/proactor.h>
#include <bco/coroutine/task.h>

namespace bco
{

class Context {
public:
    Context() = default;
    Context(std::unique_ptr<ExecutorInterface>&& executor)
        : executor_(std::move(executor))
    {
        executor_->set_proactor_task_getter(std::bind(&Context::get_proactor_tasks, this));
    }
    void start()
    {
        executor_->start();
    }
    void spawn(std::function<Task<>()>&& coroutine)
    {
        executor_->post(PriorityTask {0, std::bind(&Context::spawn_aux1, this, coroutine) });
    }
    void add_proactor(std::unique_ptr<ProactorInterface>&& proactor)
    {
        proactors_.push_back(std::move(proactor));
    }

private:
    std::vector<PriorityTask> get_proactor_tasks()
    {
        //TODO 优化
        std::vector<PriorityTask> completed_tasks;
        for (auto& p : proactors_) {
            std::ranges::copy(p->harvest_completed_tasks(), std::back_inserter(completed_tasks));
        }
        return completed_tasks;
    }
    void spawn_aux1(std::function<Task<>()> coroutine)
    {
        spawn_aux2(std::move(coroutine));
    }
    RootTask spawn_aux2(std::function<Task<>()>&& coroutine)
    {
        co_await coroutine();
    }

private:
    std::vector<std::unique_ptr<ProactorInterface>> proactors_;
    std::unique_ptr<ExecutorInterface> executor_;
};

} // namespace bco
