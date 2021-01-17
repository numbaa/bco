#include <functional>
#include <chrono>
#include <thread>
#include <iostream>
#include <bco/executor/simple_executor.h>
#include <bco/context.h>

namespace bco {

SimpleExecutor::~SimpleExecutor()
{
    //TODO: implement
}

void SimpleExecutor::post(PriorityTask task)
{
    std::lock_guard<std::mutex> lock { mutex_ };
    tasks_.push_back(task);
}

void SimpleExecutor::post_delay(std::chrono::microseconds duration, PriorityTask task)
{
    //TODO: implement
    (void)duration;
    (void)task;
}

void SimpleExecutor::start()
{
    thread_ = std::thread {std::bind(&SimpleExecutor::do_start, this)};
    std::unique_lock lock { mutex_ };
    cv_.wait(lock, [this]() { return started_; });
}

void SimpleExecutor::do_start()
{
    {
        std::lock_guard lock { mutex_ };
        started_ = true;
    }
    cv_.notify_one();

    while (true) {
        mutex_.lock();
        auto old_tasks = std::move(tasks_);
        mutex_.unlock();
        auto proactor_tasks = get_proactor_task_();
        if (old_tasks.empty()) {
            //std::this_thread::yield();
            std::this_thread::sleep_for(std::chrono::milliseconds { 1 });
        }
        for (auto&& task : old_tasks) {
            task();
        }
        for (auto&& task : proactor_tasks) {
            task();
        }
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

} //namespace bco