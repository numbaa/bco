#pragma once
#include <functional>
#include <deque>
#include <mutex>

namespace bco {

class Executor {
public:
    void post(std::function<void()> func);
    void run();
private:
    std::deque<std::function<void()>> tasks_;
    std::mutex mutex_;
};

} //namespace bco