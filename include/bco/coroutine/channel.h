#pragma once
#include <deque>
#include <mutex>
#include <bco/utils.h>
#include <bco/context.h>
#include "task.h"

namespace bco {

template <typename T>
class Channel {
public:
    Channel() = default;
    void send(T value)
    {
        std::lock_guard lock { mtx_ };
        if (pending_tasks_.empty()) {
            ready_values_.push_back(value);
        } else {
            Item item = pending_tasks_.front();
            pending_tasks_.pop_front();
            item.task.set_result(std::move(value));
            std::shared_ptr<bco::Context> ctx = item.ctx.lock();
            if (ctx != nullptr) {
                ctx->spawn([item]() mutable -> Routine { co_await item.task; });
            }
        }
    }
    Task<T> recv()
    {
        std::lock_guard lock { mtx_ };
        if (ready_values_.empty()) {
            Item item;
            item.ctx = get_current_context();
            pending_tasks_.push_back(item);
            return item.task;
        } else {
            Task<T> task;
            T value = ready_values_.front();
            ready_values_.pop_front();
            task.set_result(std::move(value));
            return task;
        }
    }

private:
    // Context ctx_;
    struct Item {
        Task<T> task;
        std::weak_ptr<Context> ctx;
    };
    std::deque<Item> pending_tasks_;
    std::deque<T> ready_values_;
    std::mutex mtx_;
};

}