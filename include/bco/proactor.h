#pragma once
#include <vector>
#include <functional>

namespace bco {

struct PriorityTask {
    int priority;
    std::function<void()> task;
    void operator()() { task(); }
};

class ProactorInterface {
public:
    virtual std::vector<PriorityTask> harvest_completed_tasks() = 0;
};

}