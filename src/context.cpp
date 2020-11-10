#include <bco/context.h>
#include <bco/task.h>

namespace bco {

void Context::loop()
{}

void Context::spawn(std::function<Task<>()>&& coroutine)
{
    executor_.post(std::bind(&Context::spawn_aux, this, std::move(coroutine)));
}

void Context::set_executor(Executor&& executor)
{
    executor_ = std::move(executor);
}

void Context::set_proactor(Proactor&& proactor)
{
    proactor_ = std::move(proactor);
}

Executor* Context::executor()
{
    return &executor_;
}

Proactor* Context::proactor()
{
    return &proactor_;
}

Task<> Context::spawn_aux(std::function<Task<>()>&& coroutine)
{
    co_await coroutine();
}

} // namespace bco