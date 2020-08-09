#include <bco/context.h>
#include <chrono>
#include <thread>

namespace bco {

void Context::post(std::function<void()> func)
{
    std::lock_guard<std::mutex> lock { mutex_ };
    tasks_.push_back(func);
}

void Context::run()
{
    while (true) {
        mutex_.lock();
        auto old_tasks = std::move(tasks_);
        mutex_.unlock();
        if (old_tasks.empty()) {
            std::this_thread::yield();
            continue;
        }
        for (auto&& task : old_tasks) {
            task();
        }
    }
}

} //namespace bco