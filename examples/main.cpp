#include <bco/bco.h>

int main()
{
    auto executor = bco::Executor{};
    executor.post([]() {
        //
    });
    executor.run();
}