#include <bco/bco.h>

int main()
{
    bco::Executor executor {};
    executor.post([]() {
        //
    });
    executor.run();
}