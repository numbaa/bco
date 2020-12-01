#pragma once
#ifndef _WIN32

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

#endif // _WIN32
