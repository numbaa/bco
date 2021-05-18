#include <functional>
#include <chrono>
#include <thread>
#include <iostream>
#include <bco/executor/simple_executor.h>
#include <bco/context.h>

namespace bco {

SimpleExecutor::~SimpleExecutor()
{
    {
        std::lock_guard lock { mutex_ };
        stoped_ = true;
    }
    sleep_cv_.notify_one();
    thread_.join();
}

void SimpleExecutor::post(PriorityTask task)
{
    std::lock_guard<std::mutex> lock { mutex_ };
    tasks_.push_back(task);
    //notify here ?
}

void SimpleExecutor::post_delay(std::chrono::milliseconds duration, PriorityTask task)
{
    std::lock_guard<std::mutex> lock { mutex_ };
    delay_tasks_.push({ duration, task });
}

void SimpleExecutor::start()
{
    thread_ = std::thread {std::bind(&SimpleExecutor::do_start, this)};
    std::unique_lock lock { startup_mtx_ };
    startup_cv_.wait(lock, [this]() { return started_; });
}

void SimpleExecutor::do_start()
{
    {
        std::lock_guard lock { startup_mtx_ };
        started_ = true;
    }
    set_current_thread_context(ctx_);
    startup_cv_.notify_one();

    while (!stoped_) {
        auto old_tasks = get_pending_tasks();
        auto [delay_tasks, sleep_for] = get_timeup_delay_tasks();
        auto proactor_tasks = get_proactor_tasks();

        if (old_tasks.empty() && delay_tasks.empty() && proactor_tasks.empty()) {
            std::unique_lock lock { mutex_ };
            wakeup_.store(false, std::memory_order::relaxed);
            sleep_cv_.wait_for(lock, sleep_for, [this]() { return wakeup_.load(std::memory_order::relaxed); });
            continue;
        }

        for (auto&& task : delay_tasks) {
            task();
        }
        for (auto&& task : old_tasks) {
            task();
        }
        for (auto&& task : proactor_tasks) {
            task();
        }
    }
}

void SimpleExecutor::wake_up()
{
    {
        std::lock_guard lock { mutex_ };
        wakeup_ = true;
    }
    sleep_cv_.notify_one();
}

std::deque<PriorityTask> SimpleExecutor::get_pending_tasks()
{
    std::lock_guard lock { mutex_ };
    return std::move(tasks_);
}

std::tuple<std::vector<PriorityTask>, std::chrono::milliseconds> SimpleExecutor::get_timeup_delay_tasks()
{
    std::vector<PriorityTask> tasks;
    auto now = std::chrono::steady_clock::now();
    std::lock_guard lock { mutex_ };
    while (!delay_tasks_.empty() && delay_tasks_.top().run_at <= now) {
        tasks.push_back(delay_tasks_.top());
        delay_tasks_.pop();
    }
    if (!delay_tasks_.empty()) {
        return { tasks, std::chrono::duration_cast<std::chrono::milliseconds>(delay_tasks_.top().run_at - now) };
    } else {
        return { tasks, std::chrono::milliseconds { 10 } };
    }
}

inline std::vector<PriorityTask> SimpleExecutor::get_proactor_tasks()
{
    if (get_proactor_task_ == nullptr) {
        return {};
    } else {
        return get_proactor_task_();
    }
}

void SimpleExecutor::set_proactor_task_getter(std::function<std::vector<PriorityTask>()> func)
{
    get_proactor_task_ = func;
}

bool SimpleExecutor::is_current_executor()
{
    return std::this_thread::get_id() == thread_.get_id();
}

void SimpleExecutor::set_context(std::weak_ptr<Context> ctx)
{
    ctx_ = ctx;
}

void SimpleExecutor::wake()
{
    wake_up();
}

bool SimpleExecutor::is_running()
{
    return wakeup_.load(std::memory_order::relaxed);
}

} //namespace bco