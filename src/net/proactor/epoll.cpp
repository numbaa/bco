#include <sys/eventfd.h>
#include <unistd.h>

#include <array>

#include <bco/exception.h>
#include <bco/net/proactor/epoll.h>

namespace bco {

namespace net {

Epoll::Epoll()
{
    epoll_fd_ = ::epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd_ < 0)
        throw NetworkException { "create epoll fd failed" };
    exit_fd_ = ::eventfd(0, EFD_NONBLOCK);
    if (exit_fd_ < 0)
        throw NetworkException { "create eventfd failed" };
    epoll_event event {};
    event.events = EPOLLIN;
    event.data.fd = exit_fd_;
    int ret = ::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, exit_fd_, &event);
    if (ret < 0)
        throw NetworkException { "add exit eventfd to epoll failed" };
}

Epoll::~Epoll()
{
    harvest_thread_.join();
}

void Epoll::start()
{
    harvest_thread_ = std::move(std::thread { std::bind(&Epoll::epoll_loop, this) });
}

void Epoll::stop()
{
    int buff {};
    ::write(exit_fd_, &buff, sizeof(buff));
}

int Epoll::read(int s, std::span<std::byte> buff, std::function<void(int length)> cb)
{
    std::lock_guard lock { mtx_ };
    auto it = pending_tasks_.find(s);
    if (it != pending_tasks_.cend()) {
        it->second.event.events |= EPOLLIN;
    } else {
        EpollTask task {};
        task.event.data.fd = s;
        task.event.events = EPOLLIN; //level trigger
        task.read = EpollItem { buff, cb };
        pending_tasks_[s] = task;
    }
    return 0;
}

void Epoll::epoll_loop()
{
    constexpr int kMaxEvents = 512;
    std::array<epoll_event, kMaxEvents> events {};
    while (true) {
        int timeout_ms = next_timeout();
        int count = epoll_wait(epoll_fd_, events.data(), events.size(), timeout_ms);
        if (count < 0 && errno != EINTR)
            return;
        for (int i = 0; i < count; i++) {
            if (events[i].data.fd == exit_fd_)
                return;
            on_io_event(events[i]);
        }
    }
}

void Epoll::on_io_event(const epoll_event& event)
{
    if (event.events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
        //??
    }
    if (event.events & EPOLLIN) {
        do_read(event);
    }
    if (event.events & EPOLLOUT) {
        //writable
    }
}

void Epoll::do_read(const epoll_event& event)
{
    auto it = flying_tasks_.find(event.data.fd);
    if (it == flying_tasks_.cend() || !it->second.read.has_value())
        return; // error handling
    auto& task = it->second.read.value();
    int bytes = ::read(event.data.fd, reinterpret_cast<void*>(task.buff.data()), task.buff.size());
    if (bytes >= 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
        std::lock_guard lock { mtx_ };
        uint32_t epollin = EPOLLIN;
        it->second.event.events &= ~epollin;
        int ret = ::epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, event.data.fd, &(it->second.event));
        if (ret < 0) {
            //TODO: error handling
            //completed_task_.push_back();
            return;
        }
        completed_task_.push_back(PriorityTask { 0, std::bind(task.cb, bytes) });
    }
}

} // namespace net

} // namespace bco