#include <bco/io.h>
#include <bco/buffer.h>

namespace bco {

TcpSocket::TcpSocket(Proactor* proactor, int fd)
    : proactor_(proactor), socket_(fd)
{
}

ProactorTask<int> TcpSocket::read(Buffer buffer)
{
    ProactorTask<int> task;
    int size = proactor_->read(socket_, buffer, [task](int length) mutable {
        task.set_result(std::forward<int>(length));
        task.resume();
    });
    if (size > 0) {
        task.set_result(std::forward<int>(size));
    }
    return task;
}

ProactorTask<int> TcpSocket::write(Buffer buffer)
{
    ProactorTask<int> task;
    int size = proactor_->write(socket_, buffer, [task](int length) mutable {
        task.set_result(std::forward<int>(length));
        task.resume();
    });
    if (size > 0) {
        task.set_result(std::forward<int>(size));
    }
    return task;
}

ProactorTask<TcpSocket> TcpSocket::accept()
{
    ProactorTask<TcpSocket> task;
    auto proactor = proactor_;
    int fd = proactor_->accept(socket_, [task, proactor](int fd) mutable {
        TcpSocket s{proactor, fd};
        task.set_result(std::move(s));
        task.resume();
    });
    if (fd > 0) {
        TcpSocket s{proactor, fd};
        task.set_result(std::move(s));
    }
    return task;
}

int TcpSocket::bind()
{
    return 0;
}

} // namespace bco
