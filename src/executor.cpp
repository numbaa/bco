#include <thread>
#include <chrono>
#include <bco/executor.h>

namespace bco {

void Executor::post(std::function<void()> func)
{
    std::lock_guard<std::mutex> lock{mutex_};
    tasks_.push_back(func);
}

void Executor::run()
{
    while (true)
    {
        mutex_.lock();
        auto old_tasks = std::move(tasks_);
        mutex_.unlock();
        if (old_tasks.empty())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        for (auto&& task : old_tasks)
        {
            task();
        }
    }
}

} //namespace bco