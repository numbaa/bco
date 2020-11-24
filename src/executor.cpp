#include <bco/executor.h>
#include <bco/context.h>
#include <chrono>
#include <thread>

namespace bco {

void Executor::post(std::function<void()>&& func)
{
    std::lock_guard<std::mutex> lock { mutex_ };
    tasks_.push_back(func);
}

void Executor::set_context(Context* ctx)
{
    context_ = ctx;
}

void Executor::run()
{
    while (true) {
        mutex_.lock();
        auto old_tasks = std::move(tasks_);
        mutex_.unlock();
        auto proactor_tasks = context_->get_proactor_tasks();
        if (old_tasks.empty() && proactor_tasks.empty()) {
            //std::this_thread::yield();
            std::this_thread::sleep_for(std::chrono::milliseconds { 2 });
            continue;
        }
        for (auto&& task : old_tasks) {
            task();
        }
        for (auto&& task : proactor_tasks) {
            task();
        }
    }
}

} //namespace bco