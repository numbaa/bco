#pragma once
#include <bco/executor.h>
#include <bco/proactor.h>
#include <bco/task.h>

namespace bco
{

template <Proactor P, Executor E>
class Context {
public:
    Context(std::unique_ptr<P>&& proactor, std::unique_ptr<E>&& executor)
        : proactor_(std::move(proactor))
        , executor_(std::move(executor))
    {
        executor_->set_proactor_task_getter(std::bind(&Context::get_proactor_tasks, this));
    }
    void loop()
    {
        executor_->run();
    }
    void spawn(std::function<Task<>()>&& coroutine)
    {
        executor_->post(std::bind(&Context::spawn_aux1, this, coroutine));
    }
    /*
    void set_executor(std::unique_ptr<SimpleExecutor>&& executor)
    {
        executor_ = std::move(executor);
    }
    void set_proactor(std::unique_ptr<P>&& proactor)
    {
        proactor_ = std::move(proactor);
    }
    */
    E* executor()
    {
        return executor_.get();
    }
    P* proactor()
    {
        return proactor_.get();
    }

private:
    std::vector<std::function<void()>> get_proactor_tasks()
    {
        return proactor_->drain(1);
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
    std::unique_ptr<E> executor_;
    std::unique_ptr<P> proactor_;
};

} // namespace bco
