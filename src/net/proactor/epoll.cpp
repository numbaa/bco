#include <sys/eventfd.h>
#include <unistd.h>

#include <cassert>

#include <array>

#include "../../common.h"
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
}

int Epoll::create(int domain, int type)
{
    auto fd = ::socket(domain, type, 0);
    if (fd < 0)
        return static_cast<int>(fd);
    set_non_block(static_cast<int>(fd));
    return static_cast<int>(fd);
}

void Epoll::start(ExecutorInterface* executor)
{
    io_executor_ = executor;
    io_executor_->post(bco::PriorityTask {
        .priority = Priority::Medium,
        .task = std::bind(&Epoll::do_io, this) });
}

void Epoll::stop()
{
    int buff {};
    ::write(exit_fd_, &buff, sizeof(buff));
}

int Epoll::recv(int s, bco::Buffer buff, std::function<void(int)> cb)
{
    std::lock_guard lock { mtx_ };
    auto it = pending_tasks_.find(s);
    if (it != pending_tasks_.cend()) {
        it->second.event.events |= EPOLLIN;
        it->second.action |= Action::Recv;
        it->second.read.emplace(buff, cb, nullptr);
    } else {
        EpollTask task {};
        task.event.data.fd = s;
        task.event.events = EPOLLIN; //level trigger
        task.action |= Action::Recv;
        task.read.emplace(buff, cb, nullptr);
        pending_tasks_[s] = task;
    }
    return 0;
}

int Epoll::recvfrom(int s, bco::Buffer buff, std::function<void(int, const sockaddr_storage&)> cb, void*)
{
    std::lock_guard lock { mtx_ };
    auto it = pending_tasks_.find(s);
    if (it != pending_tasks_.cend()) {
        it->second.event.events |= EPOLLIN;
        it->second.action |= Action::Recvfrom;
        it->second.read.emplace(buff, nullptr, cb);
    } else {
        EpollTask task {};
        task.event.data.fd = s;
        task.event.events = EPOLLIN; //level trigger
        task.action |= Action::Recvfrom;
        task.read.emplace(buff, nullptr, cb);
        pending_tasks_[s] = task;
    }
    return 0;
}

int Epoll::send(int s, bco::Buffer buff, std::function<void(int)> cb)
{
    if (io_executor_->is_current_executor()) {
        return send_sync(s, buff, cb);
    } else {
        return send_async(s, buff, cb);
    }
}

int Epoll::accept(int s, std::function<void(int, const sockaddr_storage&)> cb)
{
    EpollTask task {};
    task.event.data.fd = s;
    task.event.events = EPOLLIN; //level trigger
    task.action |= Action::Accept;
    task.read.emplace(std::span<std::byte> {}, nullptr, cb);
    std::lock_guard lock { mtx_ };
    pending_tasks_[s] = task;
    return 0;
}

int Epoll::connect(int s, const sockaddr_storage& addr, std::function<void(int)> cb)
{
    int ret = ::connect(s, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
    if (ret < 0)
        return ret;
    EpollTask task {};
    task.event.data.fd = s;
    task.event.events = EPOLLOUT; //level triger
    task.action |= Action::Connect;
    task.write.emplace(std::span<std::byte> {}, cb, nullptr);
    std::lock_guard lock { mtx_ };
    pending_tasks_[s] = task;
    return 0;
}

int Epoll::connect(int s, const sockaddr_storage& addr)
{
    return ::connect(s, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
}

int Epoll::next_timeout()
{
    return 1;
}

std::map<int, Epoll::EpollTask> Epoll::get_pending_tasks()
{
    std::lock_guard lock { mtx_ };
    return std::move(pending_tasks_);
}

void Epoll::submit_tasks(std::map<int, Epoll::EpollTask>& pending_tasks)
{
    //do some validation?
    for (auto& task : pending_tasks) {
        auto it = flying_tasks_.find(task.second.event.data.fd);
        if (it != flying_tasks_.cend()) {
            if ((task.second.action & Action::Recv) != Action::None) {
                it->second.action |= Action::Recv;
                it->second.event.events |= EPOLLIN;
                it->second.read = task.second.read;
            } else if ((task.second.action & Action::Recvfrom) != Action::None) {
                it->second.action |= Action::Recvfrom;
                it->second.event.events |= EPOLLIN;
                it->second.read = task.second.read;
            }
            if ((task.second.action & Action::Send) != Action::None) {
                it->second.action |= Action::Send;
                it->second.event.events |= EPOLLOUT;
                it->second.write = task.second.write;
                //continue;
            } else if ((task.second.action & Action::Accept) != Action::None) {
                it->second.action |= Action::Recv;
                it->second.event.events |= EPOLLIN;
                it->second.read = task.second.read;
                //continue;
            } else if ((task.second.action & Action::Connect) != Action::None) {
                it->second.action |= Action::Send;
                it->second.event.events |= EPOLLOUT;
                it->second.write = task.second.write;
            }
            int ret = epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, task.second.event.data.fd, &task.second.event);
            if (ret < 0) {
                //error handling
            }
        } else {
            flying_tasks_[task.second.event.data.fd] = task.second;
            int ret = epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, task.second.event.data.fd, &task.second.event);
            if (ret < 0) {
                //error handling
            }
        }
    }
}

void Epoll::do_io()
{
    assert(io_executor_->is_current_executor());

    constexpr int kMaxEvents = 512;
    std::array<epoll_event, kMaxEvents> events;
    auto pending_tasks = get_pending_tasks();
    submit_tasks(pending_tasks);
    constexpr int timeout = 0;
    int count = epoll_wait(epoll_fd_, events.data(), events.size(), timeout);
    if (count < 0 && errno != EINTR) {
        return;
    }
    for (int i = 0; i < count; i++) {
        if (events[i].data.fd == exit_fd_)
            return;
        on_io_event(events[i]);
    }
    using namespace std::chrono_literals;
    io_executor_->post_delay(1ms, bco::PriorityTask { .priority = Priority::Medium, .task = std::bind(&Epoll::do_io, this) });
}

int Epoll::send_sync(int s, bco::Buffer buff, std::function<void(int)> cb)
{
    int bytes = syscall_sendv(s, buff);
    if (bytes >= 0)
        return bytes;
    else if (last_error() == EAGAIN || last_error() == EWOULDBLOCK)
        return send_async(s, buff, cb);
    else
        return -last_error();
}

int Epoll::send_async(int s, bco::Buffer buff, std::function<void(int)> cb)
{
    std::lock_guard lock { mtx_ };
    auto it = pending_tasks_.find(s);
    if (it != pending_tasks_.cend()) {
        it->second.event.events |= EPOLLOUT;
        it->second.action |= Action::Send;
        it->second.write.emplace(buff, cb, nullptr);
    } else {
        EpollTask task {};
        task.event.data.fd = s;
        task.event.events = EPOLLOUT; //level triger
        task.action |= Action::Send;
        task.write.emplace(buff, cb, nullptr);
        pending_tasks_[s] = task;
    }
    return 0;
}

void Epoll::on_io_event(const epoll_event& event)
{
    if (event.events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
        //error handling
        return;
    }
    auto it = flying_tasks_.find(event.data.fd);
    if (it == flying_tasks_.cend() || !it->second.read.has_value()) {
        // error handling
        return;
    }
    if (((it->second.action & Action::Accept) != Action::None) && (event.events & EPOLLIN)) {
        do_accept(it->second);
        return;
    }
    if (((it->second.action & Action::Connect) != Action::None) && (event.events & EPOLLOUT)) {
        on_connected(it->second);
        return;
    }
    if (event.events & EPOLLIN) {
        if ((it->second.action & Action::Recv) != Action::None) {
            do_recv(it->second);
        } else if ((it->second.action & Action::Recvfrom) != Action::None) {
            do_recvfrom(it->second);
        } else {
            // TODO: error handling
        }
    }
    if (event.events & EPOLLOUT) {
        do_send(it->second);
    }
}

void Epoll::do_send(EpollTask& task)
{
    auto& ioitem = task.write.value();
    int bytes = syscall_sendv(task.event.data.fd, ioitem.buff);
    if (bytes >= 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
        std::lock_guard lock { mtx_ };
        uint32_t epollout = EPOLLOUT;
        task.event.events &= ~epollout;
        int ret = ::epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, task.event.data.fd, &(task.event));
        if (ret < 0) {
            //TODO: error handling
            //completed_task_.push_back();
            return;
        }
        completed_task_.push_back(PriorityTask { Priority::Medium, std::bind(ioitem.cb, bytes) });
    }
}

void Epoll::do_accept(EpollTask& task)
{
    auto& ioitem = task.read.value();
    sockaddr_storage addr {};
    socklen_t len = sizeof(addr);
    int fd = ::accept(task.event.data.fd, reinterpret_cast<sockaddr*>(&addr), &len);
    if (fd >= 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
        set_non_block(fd);
        std::lock_guard lock { mtx_ };
        uint32_t epollin = EPOLLIN;
        task.event.events &= ~epollin;
        int ret = ::epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, task.event.data.fd, &(task.event));
        if (ret < 0) {
            //TODO: error handling
            //completed_task_.push_back();
            return;
        }
        completed_task_.push_back(PriorityTask { Priority::Medium, std::bind(ioitem.cb, fd) });
    }
}

void Epoll::on_connected(EpollTask& task)
{
    std::lock_guard lock { mtx_ };
    uint32_t epollout = EPOLLOUT;
    task.event.events &= ~epollout;
    int ret = ::epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, task.event.data.fd, &(task.event));
    if (ret < 0) {
        //TODO: error handling
        //completed_task_.push_back();
        return;
    }
    completed_task_.push_back(PriorityTask { Priority::Medium, std::bind(task.write.value().cb, static_cast<int>(task.event.data.fd)) });
}

void Epoll::do_recv(EpollTask& task)
{
    auto& ioitem = task.read.value();
    int bytes = syscall_recvv(task.event.data.fd, ioitem.buff);
    if (bytes >= 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
        std::lock_guard lock { mtx_ };
        uint32_t epollin = EPOLLIN;
        task.event.events &= ~epollin;
        int ret = ::epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, task.event.data.fd, &(task.event));
        if (ret < 0) {
            //TODO: error handling
            //completed_task_.push_back();
            return;
        }
        completed_task_.push_back(PriorityTask { Priority::Medium, std::bind(ioitem.cb, bytes) });
    }
}

void Epoll::do_recvfrom(EpollTask& task)
{
    auto& ioitem = task.read.value();
    sockaddr_storage addr;
    socklen_t len = sizeof(addr);
    int bytes = syscall_recvmsg(task.event.data.fd, ioitem.buff, addr);
    if (bytes >= 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
        std::lock_guard lock { mtx_ };
        uint32_t epollin = EPOLLIN;
        task.event.events &= ~epollin;
        int ret = ::epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, task.event.data.fd, &(task.event));
        if (ret < 0) {
            //TODO: error handling
            //completed_task_.push_back();
            return;
        }
        completed_task_.push_back(PriorityTask { Priority::Medium, std::bind(ioitem.cb2, bytes, addr) });
    }
}

std::vector<PriorityTask> Epoll::harvest()
{
    std::lock_guard lock { mtx_ };
    return std::move(completed_task_);
}

} // namespace net

} // namespace bco