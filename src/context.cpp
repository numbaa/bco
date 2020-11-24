#include <bco/context.h>
#include <bco/task.h>

namespace bco {

void Context::loop()
{
    executor_->run();
}

void Context::spawn(std::function<Task<>()> coroutine)
{
    executor_->post(std::bind(&Context::spawn_aux1, this, coroutine));
}

void Context::set_executor(std::unique_ptr<Executor>&& executor)
{
    executor_ = std::move(executor);
    //FIXME: 
    executor_->set_context(this);
}

void Context::set_proactor(std::unique_ptr<Proactor>&& proactor)
{
    proactor_ = std::move(proactor);
}

Executor* Context::executor()
{
    return executor_.get();
}

Proactor* Context::proactor()
{
    return proactor_.get();
}

std::vector<std::function<void()>> Context::get_proactor_tasks()
{
    return proactor_->drain(0);
}

void Context::spawn_aux1(std::function<Task<>()> coroutine)
{
    spawn_aux2(coroutine);
}

Task<> Context::spawn_aux2(std::function<Task<>()> coroutine)
{
    co_await coroutine();
}

} // namespace bco