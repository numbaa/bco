#pragma once
#include <chrono>
#include <functional>
#include <type_traits>
//#include <concepts>

#include "task.h"
#include "bco/executor.h"

namespace bco {

Task<> switch_to(ExecutorInterface& executor);

template <typename Rep, typename Period>
Task<> sleep_for(std::chrono::duration<Rep, Period> duration);

template <typename Function, typename... Args>
Task<std::invoke_result<Function, Args...>> run_with(ExecutorInterface& executor, Function func, Args&&... args);

}