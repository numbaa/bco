#pragma once
#include <deque>
#include <functional>
#include <mutex>

namespace bco {
class Context;
class Executor {
public:
    Executor() = default;
    Executor(Executor&&) = delete;
    Executor& operator=(Executor&&) = delete;
    Executor(Executor&) = delete;
    Executor& operator=(Executor&) = delete;
    void set_context(Context* ctx);
    void post(std::function<void()>&& func);
    void run();
private:

private:
    std::deque<std::function<void()>> tasks_;
    std::mutex mutex_;
    Context* context_;
};

} //namespace bco