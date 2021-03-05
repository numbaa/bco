#pragma once
#include <deque>
#include <mutex>
#include "task.h"

namespace bco {

template <typename T>
class Channel {
public:
    Channel(); //set context
    void send(T value)
    {
        std::lock_guard lock { mtx_ };
        if (pending_tasks_.empty()) {
            ready_values_.push_back(value);
        } else {
            ProactorTask<T> task = pending_tasks_.front();
            pending_tasks_.pop_front();
            task.set_result(std::move(value));
            //ctx_->spawn([]()->Task<>{ co_await task;}); //wake up suspended task
        }
    }
    ProactorTask<T> recv()
    {
        ///assert(is_current_context());
        std::lock_guard lock { mtx_ };
        if (ready_.empty()) {
            ProactorTask<T> task;
            pending_tasks_.push_back(task);
            return task;
        } else {
            ProactorTask<T> task;
            T value = ready_values_.front();
            ready_values_.pop_front();
            task.set_result(std::move(value));
            return task;
        }
    }

private:
    // Context ctx_;
    std::deque<<ProactorTask<T>> pending_tasks_;
    std::deque<T> ready_values_;
    std::mutex mtx_;
};

}