#include <cassert>
#include <thread>
#include <bco/net/proactor/select.h>
#include <bco/utils.h>

namespace bco {

namespace net {

Select::Select()
{
    max_rfd_ = std::max(stop_event_.fd(), max_rfd_);
    pending_rfds_[stop_event_.fd()] = SelectTask { stop_event_.fd(), Action::Read, std::span<std::byte> {}, nullptr };
}

Select::~Select()
{
    harvest_thread_.join();
}

int Select::create_fd()
{
    auto fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return static_cast<int>(fd);
    set_non_block(static_cast<int>(fd));
    return static_cast<int>(fd);
}

void Select::start()
{
    harvest_thread_ = std::move(std::thread {std::bind(&Select::select_loop, this)});
}

void Select::stop()
{
    stop_event_.emit();
}

int Select::read(int s, std::span<std::byte> buff, std::function<void(int length)> cb)
{
    std::lock_guard lock { mtx_ };
    if (s > max_rfd_)
        max_rfd_ = s;
    pending_rfds_[s] = SelectTask { s, Action::Read, buff, cb };
    return 0;
}

int Select::write(int s, std::span<std::byte> buff, std::function<void(int length)> cb)
{
    std::lock_guard lock { mtx_ };
    if (s > max_wfd_)
        max_wfd_ = s;
    pending_wfds_[s]= SelectTask{ s, Action::Write, buff, cb };
    return 0;
}

bool Select::connect(int s, sockaddr_in addr, std::function<void(int)> cb)
{
    std::lock_guard lock { mtx_ };
    if (s > max_wfd_)
        max_wfd_ = s;
    ::connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    pending_wfds_[s] = SelectTask {s, Action::Connect, std::span<std::byte> {}, cb};
    return true;
}

int Select::accept(int s, std::function<void(int s)> cb)
{
    std::lock_guard lock { mtx_ };
    if (s > max_rfd_)
        max_rfd_ = s;
    pending_rfds_[s] = SelectTask { s, Action::Accept, std::span<std::byte> {}, cb };
    return 0;
}

void Select::on_io_event(const std::map<int, SelectTask>& tasks, const fd_set& fds)
{
    for (auto& task : tasks) {
        if (FD_ISSET(task.first, &fds))
            switch (task.second.action) {
            case Action::Accept:
                do_accept(task.second);
                break;
            case Action::Read:
                do_read(task.second);
                break;
            case Action::Write:
                do_write(task.second);
                break;
            case Action::Connect:
                on_connected(task.second);
                break;
            default:
                assert(false);
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
    return {pending_rfds_, pending_wfds_};
}

void Select::select_loop()
{
    while (true) {
        auto [ reading_fds, writing_fds ] = get_pending_io();
        fd_set rfds, wfds, efds;
        FD_ZERO(&rfds);
        FD_ZERO(&wfds);
        FD_ZERO(&efds);
        prepare_fd_set(reading_fds, rfds);
        prepare_fd_set(writing_fds, wfds);
        timeval timeout = next_timeout();
        int ret = ::select(std::max(max_rfd_, max_wfd_)+1, &rfds, &wfds, &efds, &timeout);
        if (ret < 0) {
            //TODO error handling
            continue;
        }
        if (FD_ISSET(stop_event_.fd(), &rfds)) {
            return;
        }
        on_io_event(reading_fds, rfds);
        on_io_event(writing_fds, wfds);
    }
}

void Select::do_accept(SelectTask task)
{
    sockaddr_in addr;
    int len;
    auto fd = ::accept(task.fd, nullptr, 0);
    auto e = errno;
    auto e2 = WSAGetLastError();
    if (fd >= 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
        set_non_block(static_cast<int>(fd));
        std::lock_guard lock { mtx_ };
        pending_rfds_.erase(task.fd);
        completed_task_.push_back(PriorityTask { 0, std::bind(task.cb, static_cast<int>(fd)) });
        return;
    }
    //do nothing, it will try again
}

void Select::do_read(SelectTask task)
{
    int bytes = ::recv(task.fd, reinterpret_cast<char*>(task.buff.data()), task.buff.size(), 0);
    if (bytes >= 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
        std::lock_guard lock { mtx_ };
        pending_rfds_.erase(task.fd);
        completed_task_.push_back(PriorityTask { 0, std::bind(task.cb, bytes) });
        return;
    }
    //do nothing, it will try again
}


void Select::do_write(SelectTask task)
{
    int bytes = ::send(task.fd, reinterpret_cast<const char*>(task.buff.data()), task.buff.size(), 0);
    if (bytes >= 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
        std::lock_guard lock { mtx_ };
        pending_wfds_.erase(task.fd);
        completed_task_.push_back(PriorityTask { 0, std::bind(task.cb, bytes) });
        return;
    }
    //do nothing, it will try again
}

void Select::on_connected(SelectTask task)
{
    std::lock_guard lock { mtx_ };
    pending_wfds_.erase(task.fd);
    completed_task_.push_back(PriorityTask { 0, std::bind(task.cb, task.fd) });
}

timeval Select::next_timeout()
{
    return timeval {0, 10000};
}

std::vector<PriorityTask> Select::harvest_completed_tasks()
{
    std::lock_guard lock { mtx_ };
    return std::move(completed_task_);
}


}

}