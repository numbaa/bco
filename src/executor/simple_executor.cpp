#include <functional>
#include <chrono>
#include <thread>
#include <iostream>
#include <bco/executor/simple_executor.h>
#include <bco/context.h>

namespace bco {

void SimpleExecutor::post(PriorityTask task)
{
    std::lock_guard<std::mutex> lock { mutex_ };
    tasks_.push_back(task);
}

void SimpleExecutor::start()
{
    while (true) {
        mutex_.lock();
        auto old_tasks = std::move(tasks_);
        mutex_.unlock();
        auto proactor_tasks = get_proactor_task_();
        if (old_tasks.empty()) {
            std::this_thread::yield();
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

} //namespace bco