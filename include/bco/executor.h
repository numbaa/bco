#pragma once
#include <deque>
#include <functional>
#include <mutex>

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
private:

private:
    std::deque<std::function<void()>> tasks_;
    std::mutex mutex_;
};

} //namespace bco