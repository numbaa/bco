#include <bco/io.h>

namespace bco {

Proactor::Proactor(Executor& executor)
    : executor_(executor)
{
}

TcpSocket::TcpSocket(Proactor& proactor)
    : proactor_(proactor)
{
}

Task<int> TcpSocket::read()
{
    Task<int> task;
    task.set_co_task([=](std::coroutine_handle<> resume) {
        proactor_.read([=]() mutable {
            //这Task早析构了吧
            task.set_result(0);
            resume();
        });
    });
}

Task<int> TcpSocket::write()
{
}

Task<int> TcpSocket::accept()
{
}

int TcpSocket::bind()
{
}

} // namespace bco
