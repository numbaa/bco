#include <bco/executor/simple_executor.h>
#include <bco/context.h>
#include <chrono>
#include <thread>

namespace bco {

void SimpleExecutor::post(std::function<void()>&& func)
{
    std::lock_guard<std::mutex> lock { mutex_ };
    tasks_.push_back(func);
}

void SimpleExecutor::run()
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

void SimpleExecutor::set_proactor_task_getter(std::function<std::vector<std::function<void()>>()> drain_func)
{
    get_proactor_task_ = drain_func;
}

} //namespace bco