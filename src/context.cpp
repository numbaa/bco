#include <bco/context.h>

namespace bco {

void Context::loop()
{}

void Context::spawn(std::function<CtxTask<>()>&& coroutine)
{
    spawn_aux(std::move(coroutine));
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

Task<> Context::spawn_aux(std::function<CtxTask<>()>&& coroutine)
{
    auto task = coroutine();
    task.set_executor(&executor_);
    co_await task;
}

} // namespace bco