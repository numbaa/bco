#include <bco/io.h>

namespace bco {

TcpSocket::TcpSocket(Proactor* proactor)
    : proactor_(proactor)
{
}

Task<int> TcpSocket::read(std::shared_ptr<std::vector<uint8_t>> buffer)
{
    Task<int> task;
    task.set_co_task([task, buffer, this](bco::coroutine_handle<void> resume) mutable {
        proactor_->read([task, resume, this](std::shared_ptr<std::vector<uint8_t>> buffer) mutable {
            task.set_result(buffer->size());
            resume();
        });
    });
    return task;
}

Task<int> TcpSocket::write(std::shared_ptr<std::vector<uint8_t>> buffer)
{
    Task<int> task;
    task.set_co_task([task, this](bco::coroutine_handle<void> resume) mutable {
        proactor_->read([task, resume, this](std::shared_ptr<std::vector<uint8_t>> buffer) mutable {
            task.set_result(buffer->size());
            resume();
        });
    });
    return task;
}

Task<int> TcpSocket::accept()
{
    //应该是Task<TcpSocket>
    Task<int> task;
    task.set_co_task([task, this](bco::coroutine_handle<void> resume) mutable {
        proactor_->accept([task, resume, this]() mutable {
            task.set_result(0);
            resume();
        });
    });
}

int TcpSocket::bind()
{
    return 0;
}

} // namespace bco
