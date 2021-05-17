#pragma once
#include <vector>
#include <functional>
#include <chrono>

namespace bco {

enum class Priority : uint32_t {
    Low,
    Medium,
    High,
};
inline bool operator<(const Priority& left, const Priority& right)
{
    return static_cast<std::underlying_type_t<Priority>>(left) < static_cast<std::underlying_type_t<Priority>>(right);
}

struct PriorityTask {
    Priority priority;
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
    { t.harvest() } -> std::same_as<std::vector<PriorityTask>>;
};

class ProactorInterface {
public:
    virtual ~ProactorInterface() {};
    virtual std::vector<PriorityTask> harvest() = 0;
};

} // namespace bco
