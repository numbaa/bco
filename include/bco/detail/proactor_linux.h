#pragma once

#include <functional>

namespace bco {

class Proactor {
public:
    Proactor(Executor& executor);
    void read(std::function<void()>);
    void write(std::function<void()>);
    void accept(std::function<void()>);

private:
    ::io_uring iouring_;
    Executor& executor_;
};

}