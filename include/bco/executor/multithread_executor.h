#pragma once
#include <thread>
#include <queue>
#include <vector>
#include <set>
#include <functional>
#include <condition_variable>
#include <random>

#include <bco/executor.h>
#include <bco/utils.h>

namespace bco {

//TODO: 第一版先不写成工作窃取，但是接口暂时留着
class MultithreadExecutor : public ExecutorInterface {
public:
    MultithreadExecutor(uint32_t threads = std::thread::hardware_concurrency());
    MultithreadExecutor(MultithreadExecutor&&) = delete;
    MultithreadExecutor& operator=(MultithreadExecutor&&) = delete;
    MultithreadExecutor(MultithreadExecutor&) = delete;
    MultithreadExecutor& operator=(MultithreadExecutor&) = delete;
    ~MultithreadExecutor() override;
    void post(PriorityTask task) override;
    void post_delay(std::chrono::microseconds duration, PriorityTask task) override;
    void start() override;
    void set_proactor_task_getter(std::function<std::vector<PriorityTask>()> func) override;
    bool is_current_executor() override;
    void set_context(std::weak_ptr<Context> ctx) override;
    void wake() override;
    bool is_running() override;

private:
    void main_loop();
    void worker_loop(const size_t worker_index);
    bool do_own_job(const size_t worker_index);
    bool steal_job(const size_t worker_index);
    void worker_sleep(const size_t worker_index);
    void request_proactor_task();
    size_t next_worker_index();
    std::tuple<std::vector<PriorityTask>, std::chrono::microseconds> get_timeup_delay_tasks();

private:
    class Worker {
    public:
        Worker() = default;
        ~Worker();
        std::thread::id thread_id() const;
        void set_thread(std::thread&& thread);
        void sleep_for(std::chrono::milliseconds ms);
        void wake_up();
        void post(PriorityTask task);
        std::queue<PriorityTask> pop_all_tasks();
    private:
        std::mutex mutex_;
        std::condition_variable cv_;
        std::thread thread_;
        std::queue<PriorityTask> tasks_;
    };
    const size_t worker_size_;
    std::vector<Worker> workers_;
    std::atomic<bool> stoped_ { false };
    std::mutex mutex_;
    std::condition_variable cv_;
    std::set<std::thread::id> thread_ids_;
    std::thread main_loop_thread_;
    std::priority_queue<PriorityDelayTask> delay_tasks_;
    WaitGroup wg_;
    std::weak_ptr<Context> ctx_;

    std::function<std::vector<PriorityTask>()> get_proactor_task_;

    //random
    std::mt19937 random_engine_;
    std::uniform_int_distribution<size_t> random_dis_;
};

} // namespace bco
