#pragma once
#include <deque>
#include <functional>
#include <mutex>

namespace bco {

class Context {
public:
    void post(std::function<void()> func);
    void run();

private:
    std::deque<std::function<void()>> tasks_;
    std::mutex mutex_;
};

} //namespace bco