#include <cassert>

#include <thread>

#include "../../common.h"
#include <bco/net/proactor/select.h>

namespace bco {

namespace net {

Select::SelectTask::SelectTask(int _fd, Action _action, bco::Buffer _buff, std::function<void(int)> _cb)
    : fd(_fd)
    , action(_action)
    , buff(_buff)
    , cb(_cb)
{
}

Select::SelectTask::SelectTask(int _fd, Action _action, bco::Buffer _buff, std::function<void(int, const sockaddr_storage&)> _cb)
    : fd(_fd)
    , action(_action)
    , buff(_buff)
    , cb2(_cb)
{
}

Select::Select()
{
    max_rfd_ = std::max(stop_event_.fd(), max_rfd_);
    pending_rfds_[stop_event_.fd()] = SelectTask { stop_event_.fd(), Action::Recv, bco::Buffer {}, std::function<void(int)> {} };
}

Select::~Select()
{
}

int Select::create(int domain, int type)
{
    auto fd = ::socket(domain, type, 0);
    if (fd < 0)
        return static_cast<int>(fd);
    set_non_block(static_cast<int>(fd));
    return static_cast<int>(fd);
}

void Select::start(ExecutorInterface* executor)
{
    io_executor_ = executor;
    io_executor_->post(bco::PriorityTask {
        .priority = 1,
        .task = std::bind(&Select::do_io, this) });
}

void Select::stop()
{
    stop_event_.emit();
}

int Select::recv(int s, bco::Buffer buff, std::function<void(int length)> cb)
{
    std::lock_guard lock { mtx_ };
    if (s > max_rfd_)
        max_rfd_ = s;
    pending_rfds_[s] = SelectTask { s, Action::Recv, buff, cb };
    return 0;
}

int Select::recvfrom(int s, bco::Buffer buff, std::function<void(int, const sockaddr_storage&)> cb)
{
    std::lock_guard lock { mtx_ };
    if (s > max_rfd_)
        max_rfd_ = s;
    pending_rfds_[s] = SelectTask { s, Action::Recvfrom, buff, cb };
    return 0;
}

//for tcp
int Select::send(int s, bco::Buffer buff, std::function<void(int)> cb)
{
    if (io_executor_->is_current_executor()) {
        return send_sync(s, buff, cb);
    } else {
        return send_async(s, buff, cb);
    }
}

int Select::send_sync(int s, bco::Buffer buff, std::function<void(int)> cb)
{
    int bytes = syscall_sendv(s, buff);
    if (bytes >= 0)
        return bytes;
    else if (last_error() == EAGAIN || last_error() == EWOULDBLOCK)
        return send_async(s, buff, cb);
    else
        return -last_error();
}

int Select::send_async(int s, bco::Buffer buff, std::function<void(int)> cb)
{
    std::lock_guard lock { mtx_ };
    if (s > max_wfd_)
        max_wfd_ = s;
    pending_wfds_[s] = SelectTask { s, Action::Send, buff, cb };
    return 0;
}

//for udp
int Select::send(int s, bco::Buffer buff)
{
    int bytes = syscall_sendv(s, buff);
    if (bytes == -1)
        return -last_error();
    else
        return bytes;
}

//for udp
int Select::sendto(int s, bco::Buffer buff, const sockaddr_storage& addr)
{
    int bytes = syscall_sendmsg(s, buff, addr);
    if (bytes == -1)
        return -last_error();
    else
        return bytes;
}

//for tcp
int Select::connect(int s, const sockaddr_storage& addr, std::function<void(int)> cb)
{
    int error = ::connect(s, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
    if (error == -1)
        return -last_error();
    std::lock_guard lock { mtx_ };
    if (s > max_wfd_)
        max_wfd_ = s;
    pending_wfds_[s] = SelectTask { s, Action::Connect, bco::Buffer {}, cb };
    return 0;
}

//for udp
int Select::connect(int s, const sockaddr_storage& addr)
{
    int error = ::connect(s, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
    if (error == -1)
        return -last_error();
    else
        return 0;
}

int Select::accept(int s, std::function<void(int, const sockaddr_storage&)> cb)
{
    std::lock_guard lock { mtx_ };
    if (s > max_rfd_)
        max_rfd_ = s;
    pending_rfds_[s] = SelectTask { s, Action::Accept, bco::Buffer {}, cb };
    return 0;
}

void Select::on_io_event(const std::map<int, SelectTask>& tasks, const fd_set& fds)
{
    for (auto& task : tasks) {
        if (FD_ISSET(task.first, &fds)) {
            switch (task.second.action) {
            case Action::Accept:
                do_accept(task.second);
                break;
            case Action::Recv:
                do_recv(task.second);
                break;
            case Action::Recvfrom:
                do_recvfrom(task.second);
                break;
            case Action::Send:
                do_send(task.second);
                break;
            case Action::Connect:
                on_connected(task.second);
                break;
            default:
                assert(false);
            }
        }
    }
}

void Select::prepare_fd_set(const std::map<int, SelectTask>& tasks, fd_set& fds)
{
    for (auto& task : tasks) {
        FD_SET(task.first, &fds);
    }
}

std::tuple<std::map<int, Select::SelectTask>, std::map<int, Select::SelectTask>> Select::get_pending_io()
{
    std::lock_guard lock { mtx_ };
    return { pending_rfds_, pending_wfds_ };
}

void Select::do_io()
{
    assert(io_executor_->is_current_executor());

    auto [reading_fds, writing_fds] = get_pending_io();
    fd_set rfds, wfds;
    FD_ZERO(&rfds);
    FD_ZERO(&wfds);
    prepare_fd_set(reading_fds, rfds);
    prepare_fd_set(writing_fds, wfds);
    timeval timeout {};
    int ret = ::select(std::max(max_rfd_, max_wfd_) + 1, &rfds, &wfds, nullptr, &timeout);
    if (ret < 0) {
        // TODO: error handling
    } else {
        if (FD_ISSET(stop_event_.fd(), &rfds)) {
            return;
        }
        on_io_event(reading_fds, rfds);
        on_io_event(writing_fds, wfds);
    }
    using namespace std::chrono_literals;
    io_executor_->post_delay(1ms, bco::PriorityTask { .priority = 4, .task = std::bind(&Select::do_io, this) });
}

void Select::do_accept(const SelectTask& task)
{
    sockaddr_storage addr {};
    socklen_t len = sizeof(addr);
    auto fd = ::accept(task.fd, reinterpret_cast<sockaddr*>(&addr), &len);
    if (fd >= 0 || (last_error() != EAGAIN && last_error() != EWOULDBLOCK)) {
        set_non_block(static_cast<int>(fd));
        std::lock_guard lock { mtx_ };
        pending_rfds_.erase(task.fd);
        completed_task_.push_back(PriorityTask { 0, std::bind(task.cb2, static_cast<int>(fd), addr) });
        return;
    }
    //do nothing, it will try again
}

void Select::do_recv(const SelectTask& task)
{
    int bytes = syscall_recvv(task.fd, task.buff);
    if (bytes >= 0 || (last_error() != EAGAIN && last_error() != EWOULDBLOCK)) {
        std::lock_guard lock { mtx_ };
        pending_rfds_.erase(task.fd);
        completed_task_.push_back(PriorityTask { 0, std::bind(task.cb, bytes) });
        return;
    }
    //do nothing, it will try again
}

void Select::do_recvfrom(const SelectTask& task)
{
    sockaddr_storage addr;
    int bytes = syscall_recvmsg(task.fd, task.buff, addr);
    if (bytes >= 0 || (last_error() != EAGAIN && last_error() != EWOULDBLOCK)) {
        std::lock_guard lock { mtx_ };
        pending_rfds_.erase(task.fd);
        completed_task_.push_back(PriorityTask { 0, std::bind(task.cb2, bytes, addr) });
        return;
    }
}

void Select::do_send(const SelectTask& task)
{
    int bytes = syscall_sendv(task.fd, task.buff);
    if (bytes >= 0 || (last_error() != EAGAIN && last_error() != EWOULDBLOCK)) {
        std::lock_guard lock { mtx_ };
        pending_wfds_.erase(task.fd);
        completed_task_.push_back(PriorityTask { 0, std::bind(task.cb, bytes) });
        return;
    }
    //do nothing, it will try again
}

void Select::on_connected(const SelectTask& task)
{
    std::lock_guard lock { mtx_ };
    pending_wfds_.erase(task.fd);
    completed_task_.push_back(PriorityTask { 0, std::bind(task.cb, task.fd) });
}

std::vector<PriorityTask> Select::harvest_completed_tasks()
{
    std::lock_guard lock { mtx_ };
    return std::move(completed_task_);
}

} // namespace net

} // namespace bco