#pragma once
#include <functional>
#include <vector>
#include <chrono>
#include <bco/proactor.h>

namespace bco {

namespace detail {
class ContextBase;
} // namespace detail

class ExecutorInterface {
public:
    virtual ~ExecutorInterface() {};
    virtual void post(PriorityTask task) = 0;
    virtual void post_delay(std::chrono::microseconds duration, PriorityTask task) = 0;
    virtual void start() = 0;
    virtual void set_proactor_task_getter(std::function<std::vector<PriorityTask>()> func) = 0;
    virtual bool is_current_executor() = 0;
    virtual void set_context(std::weak_ptr<detail::ContextBase> ctx) = 0;
};

} // namespace bco
