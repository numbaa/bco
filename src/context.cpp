#include <bco/context.h>

namespace bco {

void Context::loop()
{}

void Context::spawn(std::function<Task<>()> corotine)
{
    //
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

} // namespace bco