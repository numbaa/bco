#include <bco/executor/multithread_executor.h>

namespace bco {

MultithreadExecutor::MultithreadExecutor(uint32_t threads)
    : worker_size_ {std::min(std::max(threads, uint32_t{1}), uint32_t{1000})}
    , workers_ { worker_size_ }
    , wg_ { worker_size_ + 1}
    , random_engine_ { std::random_device {}() }
    , random_dis_ { 0, worker_size_-1 }
      
{
}

MultithreadExecutor::~MultithreadExecutor()
{
    stoped_ = true;
    //std::atomic_thread_fence(std::memory_order::memory_order_acquire);
    cv_.notify_one();
    for (auto& worker : workers_) {
        worker.wake_up();
    }
    main_loop_thread_.join();
}

void MultithreadExecutor::post(PriorityTask task)
{
    workers_[next_worker_index()].post(task);
}

void MultithreadExecutor::post_delay(std::chrono::microseconds duration, PriorityTask task)
{
    std::lock_guard lock { mutex_ };
    delay_tasks_.push({ duration, task });
}

void MultithreadExecutor::start()
{
    main_loop_thread_ = std::thread {std::bind(&MultithreadExecutor::main_loop, this)};
    for (size_t i = 0; i < worker_size_; i++) {
        workers_[i].set_thread(std::thread { std::bind(&MultithreadExecutor::worker_loop, this, i) });
        thread_ids_.insert(workers_[i].thread_id());
    }
    wg_.wait();
}

void MultithreadExecutor::set_proactor_task_getter(std::function<std::vector<PriorityTask>()> func)
{
    get_proactor_task_ = func;
}

bool MultithreadExecutor::is_current_executor()
{
    return thread_ids_.find(std::this_thread::get_id()) != thread_ids_.cend();
}

void MultithreadExecutor::set_context(std::weak_ptr<Context> ctx)
{
    ctx_ = ctx;
}

void MultithreadExecutor::wake()
{
    cv_.notify_one();
}

void MultithreadExecutor::main_loop()
{
    wg_.done();
    while (!stoped_) {
        auto [delay_tasks, sleep_for] = get_timeup_delay_tasks();
        auto proactor_tasks = get_proactor_task_();
        if (delay_tasks.empty() && proactor_tasks.empty()) {
            std::unique_lock lock { mutex_ };
            cv_.wait_for(lock, sleep_for);
            continue;
        }
        for (auto&& task : delay_tasks) {
            workers_[next_worker_index()].post(task);
        }
        for (auto&& task : proactor_tasks) {
            workers_[next_worker_index()].post(task);
        }
    }
}

void MultithreadExecutor::worker_loop(const size_t worker_index)
{
    wg_.done();
    while (!stoped_) {
        bool has_job = do_own_job(worker_index) || steal_job(worker_index);
        if (!has_job) {
            request_proactor_task();
            worker_sleep(worker_index);
        }
    }
}

bool MultithreadExecutor::do_own_job(const size_t worker_index)
{
    auto tasks = workers_[worker_index].pop_all_tasks();
    if (tasks.empty()) {
        return false;
    }
    while (not tasks.empty()) {
        tasks.front()();
        tasks.pop();
    }
    return true;
}

bool MultithreadExecutor::steal_job(const size_t worker_index)
{
    //‘›≤ª µœ÷
    (void)worker_index;
    return false;
}

void MultithreadExecutor::worker_sleep(const size_t worker_index)
{
    constexpr auto kSleepTime = std::chrono::milliseconds { 2 };
    workers_[worker_index].sleep_for(kSleepTime);
}

void MultithreadExecutor::request_proactor_task()
{
    cv_.notify_one();
}

size_t MultithreadExecutor::next_worker_index()
{
    return random_dis_(random_engine_);
}

std::tuple<std::vector<PriorityTask>, std::chrono::microseconds> MultithreadExecutor::get_timeup_delay_tasks()
{
    std::vector<PriorityTask> tasks;
    auto now = std::chrono::steady_clock::now();
    std::lock_guard lock { mutex_ };
    while (!delay_tasks_.empty() && delay_tasks_.top().run_at <= now) {
        tasks.push_back(delay_tasks_.top());
        delay_tasks_.pop();
    }
    if (!delay_tasks_.empty()) {
        return { tasks, std::chrono::duration_cast<std::chrono::microseconds>(delay_tasks_.top().run_at - now) };
    } else {
        return { tasks, std::chrono::microseconds { 10 } };
    }
}


MultithreadExecutor::Worker::~Worker()
{
    thread_.join();
}

std::thread::id MultithreadExecutor::Worker::thread_id() const
{
    return thread_.get_id();
}

void MultithreadExecutor::Worker::set_thread(std::thread&& thread)
{
    thread_ = std::move(thread);
}

void MultithreadExecutor::Worker::sleep_for(std::chrono::milliseconds ms)
{
    std::unique_lock lock { mutex_ };
    cv_.wait_for(lock, ms);
}

void MultithreadExecutor::Worker::wake_up()
{
    cv_.notify_one();
}

void MultithreadExecutor::Worker::post(PriorityTask task)
{
    std::lock_guard lock { mutex_ };
    tasks_.push(task);
    cv_.notify_one();
}

std::queue<PriorityTask> MultithreadExecutor::Worker::pop_all_tasks()
{
    std::lock_guard lock { mutex_ };
    return std::move(tasks_);
}

} // namespace bco
