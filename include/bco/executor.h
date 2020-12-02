#pragma once
#include <functional>
#include <vector>

namespace bco {

/*
    void post(std::function<void()>&& func);
    void run();
    void set_proactor_task_getter(std::function<std::vector<std::function<void()>>()>);
*/

template <typename T>
concept Executor = requires(T e, std::function<void()>&& func, std::function<std::vector<std::function<void()>>()> getter)
{
    e.post(std::move(func));
    e.run();
    e.set_proactor_task_getter(getter);
};

}