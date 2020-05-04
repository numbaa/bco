#pragma once
#include <experimental/coroutine>

namespace bco {

class Task {
public:
    using promise_type = promise_simple;
};

} //namespace bco