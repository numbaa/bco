#pragma once
#include <deque>
#include <functional>
#include <mutex>
#include <bco/executor.h>

namespace bco {

class SimpleExecutor : public ExecutorInterface {
public:
    SimpleExecutor() = default;
    SimpleExecutor(SimpleExecutor&&) = delete;
    SimpleExecutor& operator=(SimpleExecutor&&) = delete;
    SimpleExecutor(SimpleExecutor&) = delete;
    SimpleExecutor& operator=(SimpleExecutor&) = delete;
    void post(PriorityTask task) override;
    void start() override;
    void set_proactor_task_getter(std::function<std::vector<PriorityTask>()> func) override;

private:
    std::function<std::vector<PriorityTask>()> get_proactor_task_;
    std::deque<PriorityTask> tasks_;
    std::mutex mutex_;
};

} //namespace bco
