#pragma once
#include <vector>
#include <functional>

namespace bco {

struct PriorityTask {
    int priority;
    std::function<void()> task;
    void operator()() { task(); }
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