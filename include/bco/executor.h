#pragma once
#include <deque>
#include <functional>
#include <mutex>

namespace bco {

class Executor {
public:
    Executor();
    Executor(Executor&&);
    Executor& operator=(Executor&&);
    void post(std::function<void()>&& func);
    void run();
private:
    Executor(Executor&);
    Executor& operator=(Executor&);

private:
    std::deque<std::function<void()>> tasks_;
    std::mutex mutex_;
};

} //namespace bco