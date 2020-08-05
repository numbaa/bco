#pragma once

#include <functional>
#include <Windows.h>

namespace bco {

class Proactor {
public:
    Proactor(Executor& executor);
    void read(std::function<void()>);
    void write(std::function<void()>);
    void accept(std::function<void()>);

private:
    ::HANDLE complete_port_;
    Executor& executor_;
};

}