#include <thread>
#include <bco/proactor/select.h>

namespace bco {

int Select::read(int s, std::span<std::byte> buff, std::function<void(int length)>&& cb)
{
    std::lock_guard lock { mtx_ };
    if (s > max_rfd_)
        max_rfd_ = s;
    pending_rfds_[s] = ST { s, buff, std::bind(&Select::do_read, this, cb) };
    return 0;
}

int Select::write(int s, std::span<std::byte> buff, std::function<void(int length)>&& cb)
{
    std::lock_guard lock { mtx_ };
    if (s > max_wfd_)
        max_wfd_ = s;
    pending_wfds_[s]= ST{ s, buff, std::bind(&Select::do_write, this, cb) };
    return 0;
}

bool Select::connect(sockaddr_in addr, std::function<void(int)>&& cb)
{
    std::lock_guard lock { mtx_ };
    //
    return 0;
}

int Select::accept(int s, std::function<void(int s)>&& cb)
{
    std::lock_guard lock { mtx_ };
    if (s > max_rfd_)
        max_rfd_ = s;
    pending_rfds_[s] = ST { s, std::span<std::byte> {}, std::bind(&Select::do_accept, this, cb) };
    return 0;
}

void Select::try_do_io(const std::map<int, ST>& x, const fd_set& fds)
{
}

void Select::prepare_fd_set(const std::map<int, ST>& x, fd_set& fds)
{
    for (auto& s : x) {
        FD_SET(s.first, &fds);
    }
}

std::tuple<std::map<int, Select::ST>, std::map<int, Select::ST>> Select::get_pending_io()
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
        try_do_io(reading_fds, rfds);
        try_do_io(writing_fds, wfds);
    }
}



}