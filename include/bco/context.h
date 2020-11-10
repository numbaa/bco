#pragma once
#include <bco/executor.h>
#include <bco/proactor.h>
#include <bco/task.h>

namespace bco
{

class Context {
public:
    Context();
    void loop();
    void spawn(std::function<Task<>()>&& coroutine);
    void set_executor(Executor&& executor);
    void set_proactor(Proactor&& proactor);
    Executor* executor();
    Proactor* proactor();
private:
    Task<> spawn_aux(std::function<Task<>()>&& cooutine);
private:
    Executor executor_;
    Proactor proactor_;
};

} // namespace bco
