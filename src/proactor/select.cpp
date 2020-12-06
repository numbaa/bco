#include <cassert>
#include <thread>
#include <bco/proactor/select.h>

namespace bco {

Select::Select()
{
}

void Select::start()
{
    harvest_thread_ = std::move(std::thread {std::bind(&Select::select_loop, this)});
}

int Select::read(int s, std::span<std::byte> buff, std::function<void(int length)>&& cb)
{
    std::lock_guard lock { mtx_ };
    if (s > max_rfd_)
        max_rfd_ = s;
    pending_rfds_[s] = SelectTask { s, Action::Read, buff, cb };
    return 0;
}

int Select::write(int s, std::span<std::byte> buff, std::function<void(int length)>&& cb)
{
    std::lock_guard lock { mtx_ };
    if (s > max_wfd_)
        max_wfd_ = s;
    pending_wfds_[s]= SelectTask{ s, Action::Write, buff, cb };
    return 0;
}

bool Select::connect(int s, sockaddr_in addr, std::function<void(int)>&& cb)
{
    std::lock_guard lock { mtx_ };
    if (s > max_wfd_)
        max_wfd_ = s;
    ::connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    pending_wfds_[s] = SelectTask {s, Action::Connect, std::span<std::byte> {}, cb};
    return 0;
}

int Select::accept(int s, std::function<void(int s)>&& cb)
{
    std::lock_guard lock { mtx_ };
    if (s > max_rfd_)
        max_rfd_ = s;
    pending_rfds_[s] = SelectTask { s, Action::Accept, std::span<std::byte> {}, cb };
    return 0;
}

void Select::on_io_event(const std::map<int, SelectTask>& x, const fd_set& fds)
{
    for (auto& xx : x) {
        if (FD_ISSET(xx.first, &fds))
            switch (xx.second.action) {
            case Action::Accept:
                do_accept(xx.second);
                break;
            case Action::Read:
                do_read(xx.second);
                break;
            case Action::Write:
                do_write(xx.second);
                break;
            case Action::Connect:
                on_connected(xx.second);
                break;
            default:
                assert(false);
            }
    }
}

void Select::prepare_fd_set(const std::map<int, SelectTask>& x, fd_set& fds)
{
    for (auto& s : x) {
        FD_SET(s.first, &fds);
    }
}

std::tuple<std::map<int, Select::SelectTask>, std::map<int, Select::SelectTask>> Select::get_pending_io()
{
    std::lock_guard lock { mtx_ };
    return {pending_rfds_, pending_wfds_};
}

void Select::select_loop()
{
    while (true) {
        auto [ reading_fds, writing_fds ] = get_pending_io();
        fd_set rfds, wfds, efds;
        prepare_fd_set(reading_fds, rfds);
        prepare_fd_set(writing_fds, wfds);
        timeval timeout = next_timeout();
        int ret = ::select(std::max(max_rfd_, max_wfd_)+1, &rfds, &wfds, &efds, &timeout);
        if (ret < 0) {
            //TODO error handling
        }
        on_io_event(reading_fds, rfds);
        on_io_event(writing_fds, wfds);
    }
}

void Select::do_accept(SelectTask task)
{
    sockaddr_in addr;
    int len;
    int ret = ::accept(task.fd, reinterpret_cast<sockaddr*>(&addr), &len);
    if (ret >= 0) {
        std::lock_guard lock { mtx_ };
        pending_rfds_.erase(task.fd);
        completed_task_.push_back(std::bind(task.cb, ret));
        return;
    }
    switch (errno) {
    case EAGAIN:
    default:
    }
}

void Select::do_read(SelectTask task)
{
    int ret = ::recv(task.fd, reinterpret_cast<char*>(task.buff.data()), task.buff.size(), 0);
    if (ret >= 0) {
        std::lock_guard lock { mtx_ };
        pending_rfds_.erase(task.fd);
        completed_task_.push_back(std::bind(task.cb, ret));
        return;
    }
    switch (errno) {
    case EAGAIN:
    default:
    }
}


void Select::do_write(SelectTask task)
{
    int ret = ::send(task.fd, reinterpret_cast<const char*>(task.buff.data()), task.buff.size(), 0);
    if (ret >= 0) {
        std::lock_guard lock { mtx_ };
        pending_wfds_.erase(task.fd);
        completed_task_.push_back(std::bind(task.cb, ret));
        return;
    }
    switch (errno) {
    case EAGAIN:
    default:
    }
}

void Select::on_connected(SelectTask task)
{
    //TODO: ¥ÌŒÛ¥¶¿Ì
    std::lock_guard lock { mtx_ };
    pending_wfds_.erase(task.fd);
    completed_task_.push_back(std::bind(task.cb, task.fd));

}

timeval Select::next_timeout()
{
    return timeval();
}

std::vector<std::function<void()>> Select::drain(uint32_t timeout_ms)
{
    std::lock_guard lock { mtx_ };
    return std::move(completed_task_);
}

}