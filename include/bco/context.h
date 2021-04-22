#pragma once
#include <cassert>
#include <map>
#include <set>
#include <memory>
#include <mutex>

#include <bco/coroutine/task.h>
#include <bco/executor.h>

namespace bco {

class Context : public std::enable_shared_from_this<Context> {
public:
    Context() = default;
    Context(std::unique_ptr<ExecutorInterface>&& executor);

    void set_executor(std::unique_ptr<ExecutorInterface>&& executor);
    ExecutorInterface* executor();

    template <typename P> requires Proactor<P> void add_proactor(std::unique_ptr<P>&& proactor);
    template <typename P> requires Proactor<P> P* get_proactor();
    std::vector<PriorityTask> get_proactor_tasks();

    void start();
    void spawn(std::function<Routine()>&& coroutine);
    void add_routine(Routine routine);
    void del_routine(Routine routine);
    size_t routines_size();

private:
    void spawn_aux(std::function<Routine()> coroutine);

private:
    std::unique_ptr<ExecutorInterface> executor_;
    std::map<std::size_t, std::unique_ptr<ProactorInterface>> proactors_;

    using TimePoint = std::chrono::steady_clock::time_point;
    using Clock = std::chrono::steady_clock;
    struct RoutineInfo {
        Routine routine;
        TimePoint start_time;
        std::strong_ordering operator<=>(const RoutineInfo& other) const
        {
            return routine <=> other.routine;
        }
    };
    std::set<RoutineInfo> routines_;
    std::mutex mutex_;
};

template <typename P> requires Proactor<P>
inline void Context::add_proactor(std::unique_ptr<P>&& proactor)
{
    //proactors_[typeid(P).hash_code()] = std::move(proactor);
    proactors_.emplace(typeid(P).hash_code(), std::move(proactor));
}

template <typename P> requires Proactor<P>
inline P* Context::get_proactor()
{
#ifndef NODEBUG
    auto it = proactors_.find(typeid(P).hash_code());
    assert(it != proactors_.cend());
#endif // NODEBUG
    return static_cast<P*>(proactors_[typeid(P).hash_code()].get());
}

} // namespace bco