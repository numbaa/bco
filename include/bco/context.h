#pragma once
#include <algorithm>
#include <chrono>
#include <functional>
#include <memory>
#include <set>

#include <bco/coroutine/task.h>
#include <bco/executor.h>
#include <bco/proactor.h>

namespace bco {

namespace detail {

//TODO: 增加更多管理routine的功能
class ContextBase : public ::std::enable_shared_from_this<ContextBase> {
    using TimePoint = std::chrono::steady_clock::time_point;
    using Clock = std::chrono::steady_clock;

public:
    ContextBase() = default;
    void start()
    {
        executor_->set_context(weak_from_this());
        executor_->start();
    }
    ExecutorInterface* executor()
    {
        return executor_.get();
    }
    void add_routine(Routine routine)
    {
        std::lock_guard lock { mutex_ };
        routines_.insert(RoutineInfo { routine, Clock::now() });
    }
    void del_routine(Routine routine)
    {
        std::lock_guard lock { mutex_ };
        constexpr auto _time = TimePoint {};
        routines_.erase(RoutineInfo { routine, _time });
    }
    size_t routines_size() const
    {
        std::lock_guard { mutex_ };
        return routines_.size();
    }

protected:
    std::unique_ptr<ExecutorInterface> executor_;

private:
    struct RoutineInfo {
        Routine routine;
        TimePoint start_time;
    };
    std::mutex mutex_;
    std::set<RoutineInfo, std::less<Routine>> routines_;
};

} // namespace detail

template <typename... Types>
class Context;

template <typename T, typename... Types>
requires Proactor<T> class Context<T, Types...> : public T::GetterSetter, public Context<Types...> {
public:
    std::vector<PriorityTask> get_proactor_tasks()
    {
        auto tasks = T::GetterSetter::proactor()->harvest_completed_tasks();
        std::ranges::copy(Context<Types...>::get_proactor_tasks(), std::back_inserter(tasks));
        return tasks;
    }
};

template <typename T>
requires Proactor<T> class Context<T> : public T::GetterSetter, public detail::ContextBase {
public:
    Context() = default;
    Context(std::unique_ptr<ExecutorInterface>&& executor)
        : executor_(std::move(executor))
    {
        executor_->set_proactor_task_getter(std::bind(&Context::get_proactor_tasks, this));
    }
    void spawn(std::function<Routine()>&& coroutine)
    {
        executor_->post(PriorityTask { 0, std::bind(&Context::spawn_aux, this, coroutine) });
    }
    void set_executor(std::unique_ptr<ExecutorInterface>&& executor)
    {
        executor_ = std::move(executor);
        executor_->set_proactor_task_getter(std::bind(&Context::get_proactor_tasks, this));
    }
    std::vector<PriorityTask> get_proactor_tasks()
    {
        return T::GetterSetter::proactor()->harvest_completed_tasks();
    }

private:
    void spawn_aux(std::function<Routine()> coroutine)
    {
        coroutine();
    }
};

} // namespace bco
