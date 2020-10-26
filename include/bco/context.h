#pragma once
#include "executor.h"
#include "proactor.h"
#include "task.h"

namespace bco
{

class Context {
public:
    Context();
    void loop();
    void spawn(std::function<Task<>()> corotine);
    void set_executor(Executor&& executor);
    void set_proactor(Proactor&& proactor);
    Executor* executor();
    Proactor* proactor();
private:
    Executor executor_;
    Proactor proactor_;
};

} // namespace bco
