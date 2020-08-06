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
    task.set_co_task([task, this](bco::coroutine_handle<void> resume) mutable {
        proactor_.read([task, resume, this]() mutable {
            //这Task早析构了吧
            task.set_result(0);
            resume();
        });
    });
    return task;
}

Task<int> TcpSocket::write()
{
    return Task<int> {};
}

Task<int> TcpSocket::accept()
{
    return Task<int> {};
}

int TcpSocket::bind()
{
    return 0;
}

} // namespace bco
