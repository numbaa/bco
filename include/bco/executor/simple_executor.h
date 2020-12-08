#pragma once
#include <deque>
#include <functional>
#include <mutex>

namespace bco {

class SimpleExecutor {
public:
    SimpleExecutor() = default;
    SimpleExecutor(SimpleExecutor&&) = delete;
    SimpleExecutor& operator=(SimpleExecutor&&) = delete;
    SimpleExecutor(SimpleExecutor&) = delete;
    SimpleExecutor& operator=(SimpleExecutor&) = delete;
    void post(std::function<void()>&& func);
    void run();
    void set_proactor_task_getter(std::function<std::vector<std::function<void()>>()>);

private:
    std::function<std::vector<std::function<void()>>()> get_proactor_task_;
    std::deque<std::function<void()>> tasks_;
    std::mutex mutex_;
};

} //namespace bco
