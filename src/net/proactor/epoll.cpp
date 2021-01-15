#include <sys/eventfd.h>
#include <unistd.h>

#include <array>

#include <bco/exception.h>
#include <bco/net/proactor/epoll.h>
#include <bco/utils.h>

#include <bco/net/tcp.h>
#include <bco/net/udp.h>

namespace bco {

namespace net {

TcpSocket<Epoll> __instance_epoll_tcp;
UdpSocket<Epoll> __instance_epoll_udp;

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

int Epoll::create_fd()
{
    auto fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return static_cast<int>(fd);
    set_non_block(static_cast<int>(fd));
    return static_cast<int>(fd);
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
        it->second.action |= Action::Read;
        it->second.write.emplace(buff, cb);
    } else {
        EpollTask task {};
        task.event.data.fd = s;
        task.event.events = EPOLLIN; //level trigger
        task.action |= Action::Read;
        task.read.emplace(buff, cb);
        pending_tasks_[s] = task;
    }
    return 0;
}

int Epoll::write(int s, std::span<std::byte> buff, std::function<void(int length)> cb)
{
    /*
    if (is_current_thread(harvest_thread_)) {
        int bytes = ::write(s, reinterpret_cast<void*>(buff.data()), buff.size());
        if (bytes < 0 && should_try_again()) {
            // try again
        } else {
            //error or success
            return bytes;
        }
    }
    */
    std::lock_guard lock { mtx_ };
    auto it = pending_tasks_.find(s);
    if (it != pending_tasks_.cend()) {
        it->second.event.events |= EPOLLOUT;
        it->second.action |= Action::Write;
        it->second.write.emplace(buff, cb);
    } else {
        EpollTask task {};
        task.event.data.fd = s;
        task.event.events = EPOLLOUT; //level triger
        task.action |= Action::Write;
        task.write.emplace(buff, cb);
        pending_tasks_[s] = task;
    }
    return 0;
}

int Epoll::accept(int s, std::function<void(int s)> cb)
{
    EpollTask task {};
    task.event.data.fd = s;
    task.event.events = EPOLLIN; //level trigger
    task.action |= Action::Accept;
    task.read.emplace(std::span<std::byte> {}, cb);
    std::lock_guard lock {mtx_};
    pending_tasks_[s] = task;
    return 0;
}

bool Epoll::connect(int s, sockaddr_in addr, std::function<void(int)> cb)
{
    int ret = ::connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (ret < 0)
        return false;
    EpollTask task {};
    task.event.data.fd = s;
    task.event.events = EPOLLOUT; //level triger
    task.action |= Action::Connect;
    task.write.emplace(std::span<std::byte> {}, cb);
    std::lock_guard lock {mtx_};
    pending_tasks_[s] = task;
    return true;
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
            if ((task.second.action & Action::Read) != Action::None) {
                it->second.action |= Action::Read;
                it->second.event.events |= EPOLLIN;
                it->second.read = task.second.read;
            }
            if ((task.second.action & Action::Write) != Action::None) {
                it->second.action |= Action::Write;
                it->second.event.events |= EPOLLOUT;
                it->second.write = task.second.write;
                //continue;
            } else if ((task.second.action & Action::Accept) != Action::None) {
                it->second.action |= Action::Read;
                it->second.event.events |= EPOLLIN;
                it->second.read = task.second.read;
                //continue;
            } else if ((task.second.action & Action::Connect) != Action::None) {
                it->second.action |= Action::Write;
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

void Epoll::epoll_loop()
{
    constexpr int kMaxEvents = 512;
    std::array<epoll_event, kMaxEvents> events {};
    while (true) {
        auto pending_tasks = get_pending_tasks();
        submit_tasks(pending_tasks);
        int timeout_ms = next_timeout();
        int count = epoll_wait(epoll_fd_, events.data(), events.size(), timeout_ms);
        if (count < 0 && errno != EINTR) {
            return;
        }
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
        do_read(it->second);
        //no return here
    }
    if (event.events & EPOLLOUT) {
        do_write(it->second);
    }
}

void Epoll::do_write(EpollTask& task)
{
    auto& ioitem = task.write.value();
    int bytes = ::write(task.event.data.fd, reinterpret_cast<void*>(ioitem.buff.data()), ioitem.buff.size());
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
        completed_task_.push_back(PriorityTask { 0, std::bind(ioitem.cb, bytes) });
    }
}

void Epoll::do_accept(EpollTask& task)
{
    auto& ioitem = task.read.value();
    int fd = ::accept(task.event.data.fd, nullptr, 0);
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
        completed_task_.push_back(PriorityTask { 0, std::bind(ioitem.cb, fd) });
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
    completed_task_.push_back(PriorityTask { 0, std::bind(task.write.value().cb, static_cast<int>(task.event.data.fd)) });
}

void Epoll::do_read(EpollTask& task)
{
    auto& ioitem = task.read.value();
    int bytes = ::read(task.event.data.fd, reinterpret_cast<void*>(ioitem.buff.data()), ioitem.buff.size());
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
        completed_task_.push_back(PriorityTask { 0, std::bind(ioitem.cb, bytes) });
    }
}

std::vector<PriorityTask> Epoll::harvest_completed_tasks()
{
    std::lock_guard lock { mtx_ };
    return std::move(completed_task_);
}

} // namespace net

} // namespace bco