#pragma once
#include <vector>
#include <functional>
#include <chrono>

namespace bco {

struct PriorityTask {
    int priority;
    std::function<void()> task;
    void operator()() { task(); }
};

struct PriorityDelayTask : PriorityTask {
    PriorityDelayTask(std::chrono::microseconds _delay, PriorityTask task)
        : PriorityTask(task)
        , delay(_delay)
        , run_at(std::chrono::steady_clock::now() + delay)
    {
    }
    PriorityDelayTask(PriorityTask task, std::chrono::microseconds _delay)
        : PriorityTask(task)
        , delay(_delay)
        , run_at(std::chrono::steady_clock::now() + delay)
    {
    }
    bool operator<(const PriorityDelayTask& rhs) const
    {
        return delay < rhs.delay;
    }
    std::chrono::microseconds delay;
    std::chrono::time_point<std::chrono::steady_clock> run_at;
};

template <typename T>
concept Proactor = requires(T t) {
    { typename T::GetterSetter{}.proactor() } -> std::same_as<T*>;
    { t.harvest_completed_tasks() } -> std::same_as<std::vector<PriorityTask>>;
};

/*
class ProactorInterface {
public:
    virtual std::vector<PriorityTask> harvest_completed_tasks() = 0;
};
*/

}