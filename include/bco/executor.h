#pragma once
#include <functional>
#include <vector>
#include <chrono>
#include <bco/proactor.h>

namespace bco {

class ExecutorInterface {
public:
    virtual ~ExecutorInterface() {};
    virtual void post(PriorityTask task) = 0;
    virtual void post_delay(std::chrono::microseconds duration, PriorityTask task) = 0;
    virtual void start() = 0;
    virtual void set_proactor_task_getter(std::function<std::vector<PriorityTask>()> func) = 0;
};

ExecutorInterface* get_current_executor();

}
