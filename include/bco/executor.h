#pragma once
#include <deque>
#include <functional>
#include <mutex>
#include "bco/proactor.h"

namespace bco {

class Executor {
public:
    Executor() = default;
    Executor(Executor&&) = delete;
    Executor& operator=(Executor&&) = delete;
    Executor(Executor&) = delete;
    Executor& operator=(Executor&) = delete;
    void post(std::function<void()>&& func);
    void run();
    void set_proactor_task_getter(std::function<std::vector<std::function<void()>>()>);

private:
    std::function<std::vector<std::function<void()>>()> get_proactor_task_;
    std::deque<std::function<void()>> tasks_;
    std::mutex mutex_;
};

} //namespace bco