#pragma once
#include <queue>
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
    ~SimpleExecutor() override;
    void post(PriorityTask task) override;
    void post_delay(std::chrono::microseconds duration, PriorityTask task) override;
    void start() override;
    void set_proactor_task_getter(std::function<std::vector<PriorityTask>()> func) override;
    bool is_current_executor();
    void set_context(std::weak_ptr<Context> ctx) override;

private:
    void do_start();
    void wake_up();
    inline std::deque<PriorityTask> get_pending_tasks();
    inline std::tuple<std::vector<PriorityTask>, std::chrono::microseconds> get_timeup_delay_tasks();

private:
    std::function<std::vector<PriorityTask>()> get_proactor_task_;
    std::deque<PriorityTask> tasks_;
    std::priority_queue<PriorityDelayTask> delay_tasks_;
    std::mutex mutex_;
    std::condition_variable sleep_cv_;
    bool wakeup_ = true;
    std::mutex startup_mtx_;
    std::condition_variable startup_cv_;
    std::thread thread_;
    bool started_ = false;
    std::weak_ptr<Context> ctx_;
};

} //namespace bco
