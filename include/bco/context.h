#pragma once
#include <bco/executor.h>
#include <bco/proactor.h>
#include <bco/task.h>

namespace bco
{

class Context {
public:
    Context() = default;
    void loop();
    void spawn(std::function<Task<>()> coroutine);
    void set_executor(std::unique_ptr<Executor>&& executor);
    void set_proactor(std::unique_ptr<Proactor>&& proactor);
    Executor* executor();
    Proactor* proactor();
private:
    void spawn_aux1(std::function<Task<>()> cooutine);
    Task<> spawn_aux2(std::function<Task<>()> cooutine);

private:
    std::unique_ptr<Executor> executor_;
    std::unique_ptr<Proactor> proactor_;
};

} // namespace bco
