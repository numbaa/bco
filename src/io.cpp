#include <bco/io.h>

namespace bco {

TcpSocket::TcpSocket(Proactor* proactor)
    : proactor_(proactor)
{
}

IoTask<int> TcpSocket::read(bco::Buffer buffer)
{
    IoTask<int> task;
    int size = proactor_->read(socket_, buffer, [task](int length) mutable {
        task.set_result(std::forward<int>(length));
        task.resume();
    });
    if (size > 0) {
        task.set_result(std::forward<int>(size));
    }
    return task;
}

IoTask<int> TcpSocket::write(bco::Buffer buffer)
{
    IoTask<int> task;
    int size = proactor_->write(socket_, buffer, [task](int length) mutable {
        task.set_result(std::forward<int>(length));
        task.resume();
    });
    if (size > 0) {
        task.set_result(std::forward<int>(size));
    }
    return task;
}

IoTask<TcpSocket> TcpSocket::accept()
{
    IoTask<TcpSocket> task;
    int fd = proactor_->accept(socket_, [task](int fd) mutable {
        //TODO: create socket from fd
        TcpSocket s;
        task.set_result(std::move(s));
        task.resume();
    });
    if (fd > 0) {
        //TODO: create socket from fd
        TcpSocket s;
        task.set_result(std::move(s));
    }
    return task;
}

int TcpSocket::bind()
{
    return 0;
}

} // namespace bco
