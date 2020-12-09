#pragma once
#include <functional>
#include <vector>
#include <bco/proactor.h>

namespace bco {

class ExecutorInterface {
public:
    virtual void post(PriorityTask task) = 0;
    virtual void start() = 0;
    virtual void set_proactor_task_getter(std::function<std::vector<PriorityTask>()> func) = 0;
};

}
