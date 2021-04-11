#ifdef __linux__
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <cassert>
#include <cstring>
#include <functional>

#include "../../common.h"
#include <bco/exception.h>
#include <bco/net/proactor/iouring.h>

namespace bco {

namespace net {

IOUring::IOUring(const Params& params)
{
    setup_io_uring(params);
}

IOUring::~IOUring()
{
    if (sq_ring_.mmap_ptr == cq_ring_.mmap_ptr) {
        ::munmap(sq_ring_.mmap_ptr, sq_ring_.ring_size);
    } else {
        ::munmap(sq_ring_.mmap_ptr, sq_ring_.ring_size);
        ::munmap(cq_ring_.mmap_ptr, cq_ring_.ring_size);
    }
    ::munmap(sqes_, sq_entries_ * sizeof(::io_uring_sqe));
}

void IOUring::setup_io_uring(const Params& init_params)
{
    constexpr uint32_t kDefaultDepth = 1;
    ::io_uring_params params;
    char* sq_ptr;
    char* cq_ptr;

    ::memset(&params, 0, sizeof(params));
    fd_ = _io_uring_setup(init_params.queue_depth.value_or(kDefaultDepth), &params);
    if (fd_ < 0) {
        throw NetworkException { "create io_uring fd failed" };
    }
    //SQ ring的元素是指向SQEs元素的偏移量
    sq_ring_.ring_size = params.sq_off.array + params.sq_entries * sizeof(unsigned);
    //CQ ring的元素是io_uring_cqe
    cq_ring_.ring_size = params.cq_off.cqes + params.cq_entries * sizeof(::io_uring_cqe);
    if (params.features & IORING_FEAT_SINGLE_MMAP) {
        sq_ring_.ring_size = cq_ring_.ring_size = std::max(sq_ring_.ring_size, cq_ring_.ring_size);
    }
    auto ptr = ::mmap(nullptr, sq_ring_.ring_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, fd_, IORING_OFF_SQ_RING);
    if (ptr == MAP_FAILED) {
        throw NetworkException { "mmap failed" };
    }
    sq_ptr = static_cast<char*>(ptr);
    //kernel 5.4
    if (params.features & IORING_FEAT_SINGLE_MMAP) {
        cq_ptr = sq_ptr;
    } else {
        auto ptr = ::mmap(nullptr, cq_ring_.ring_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, fd_, IORING_OFF_CQ_RING);
        if (ptr == MAP_FAILED) {
            throw NetworkException { "mmap failed" };
        }
        cq_ptr = static_cast<char*>(ptr);
    }
    sq_ring_.head = reinterpret_cast<unsigned*>(sq_ptr + params.sq_off.head);
    sq_ring_.tail = reinterpret_cast<unsigned*>(sq_ptr + params.sq_off.tail);
    sq_ring_.ring_mask = reinterpret_cast<unsigned*>(sq_ptr + params.sq_off.ring_mask);
    sq_ring_.ring_entries = reinterpret_cast<unsigned*>(sq_ptr + params.sq_off.ring_entries);
    sq_ring_.flags = reinterpret_cast<unsigned*>(sq_ptr + params.sq_off.flags);
    sq_ring_.array = reinterpret_cast<unsigned*>(sq_ptr + params.sq_off.array);

    cq_ring_.head = reinterpret_cast<unsigned*>(cq_ptr + params.cq_off.head);
    cq_ring_.tail = reinterpret_cast<unsigned*>(cq_ptr + params.cq_off.tail);
    cq_ring_.ring_mask = reinterpret_cast<unsigned*>(cq_ptr + params.cq_off.ring_mask);
    cq_ring_.ring_entries = reinterpret_cast<unsigned*>(cq_ptr + params.cq_off.ring_entries);
    cq_ring_.cqes = reinterpret_cast<::io_uring_cqe*>(cq_ptr + params.cq_off.cqes);

    //SQEs要单独mmap
    sq_entries_ = params.sq_entries;
    void* ret = ::mmap(0, params.sq_entries * sizeof(::io_uring_sqe), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, fd_, IORING_OFF_SQES);
    if (ret == MAP_FAILED) {
        throw NetworkException { "mmap failed" };
    }
    sqes_ = static_cast<::io_uring_sqe*>(ret);
}

void IOUring::start(ExecutorInterface* executor)
{
    executor_ = executor;
    executor_->post(bco::PriorityTask {
        .priority = 1,
        .task = std::bind(&IOUring::do_io, this) });
}

void IOUring::stop()
{
}

int IOUring::create(int domain, int type)
{
    auto fd = ::socket(domain, type, 0);
    if (fd < 0)
        return static_cast<int>(fd);
    set_non_block(static_cast<int>(fd));
    return static_cast<int>(fd);
}

int IOUring::bind(int s, const sockaddr_storage& addr)
{
    return ::bind(s, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
}

int IOUring::listen(int s, int backlog)
{
    return ::listen(s, backlog);
}

int IOUring::recv(int s, bco::Buffer buff, std::function<void(int)> cb)
{
    std::lock_guard lock { mutex_ };
    pending_tasks_.push(UringTask { s, Action::Recv, buff, cb });
}

int IOUring::recvfrom(int s, bco::Buffer buff, std::function<void(int, const sockaddr_storage&)> cb)
{
    std::lock_guard lock { mutex_ };
    pending_tasks_.push(UringTask { s, Action::Recvfrom, buff, cb });
}

int IOUring::send(int s, bco::Buffer buff, std::function<void(int)> cb)
{
    std::lock_guard lock { mutex_ };
    pending_tasks_.push(UringTask { s, Action::Send, buff, cb });
}

//需不需要加入iouring??
int IOUring::send(int s, bco::Buffer buff)
{
    int bytes = syscall_sendv(s, buff);
    if (bytes == -1)
        return -last_error();
    else
        return bytes;
}

//需不需要加入iouring??
int IOUring::sendto(int s, bco::Buffer buff, const sockaddr_storage& addr)
{
    int bytes = syscall_sendmsg(s, buff, addr);
    if (bytes == -1)
        return -last_error();
    else
        return bytes;
}

int IOUring::accept(int s, std::function<void(int)> cb)
{
    std::lock_guard lock { mutex_ };
    pending_tasks_.push(UringTask { s, Action::Accept, bco::Buffer {}, cb });
}

int IOUring::connect(int s, const sockaddr_storage& addr, std::function<void(int)> cb)
{
    int error = ::connect(s, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
    if (error == -1)
        return -last_error();
    std::lock_guard lock { mutex_ };
    pending_tasks_.push(UringTask { s, Action::Connect, bco::Buffer {}, cb });
}

int IOUring::connect(int s, const sockaddr_storage& addr)
{
    int error = ::connect(s, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
    if (error == -1)
        return -last_error();
    else
        return 0;
}

std::vector<PriorityTask> IOUring::harvest_completed_tasks()
{
}

void IOUring::do_io()
{
    assert(executor_->is_current_executor());

    auto pending_tasks = get_pending_tasks();
    submit_tasks(pending_tasks);
    handle_complete_task();
    using namespace std::chrono_literals;
    executor_->post_delay(1ms, bco::PriorityTask { .priority = 1, .task = std::bind(&IOUring::do_io, this) });
}

std::queue<IOUring::UringTask> IOUring::get_pending_tasks()
{
    std::lock_guard lock { mutex_ };
    return std::move(pending_tasks_);
}

void IOUring::submit_tasks(std::queue<IOUring::UringTask>& tasks)
{
    size_t ops = 0;
    while (!tasks.empty() && true /*empty sqe*/) {
        auto& task = tasks.front();
        submit_one_task(task);
        tasks.pop();
        ops++;
    }
    int ret = _io_uring_enter(fd_, ops, 0, 0);
    if (ret < 0) {
        // TODO: error handling;
    }
}

void IOUring::submit_one_task(const IOUring::UringTask& task)
{
    const auto slices = task.buff.data();
    std::vector<::iovec> iovecs(slices.size());
    for (size_t i = 0; i < iovecs.size(); i++) {
        iovecs[i] = ::iovec { slices[i].data(), slices[i].size() };
    }
    unsigned next_tail = *sq_ring_.tail;
    unsigned tail = next_tail;
    next_tail++;
    std::atomic_thread_fence(std::memory_order::release);
    unsigned index = tail & *sq_ring_.ring_mask;
    ::io_uring_sqe* sqe = sqes_ + index;
    sqe->fd = task.fd;
    sqe->flags = 0;
    sqe->opcode = action_to_opcode(task.action);
    sqe->addr = reinterpret_cast<decltype(sqe->addr)>(iovecs.data());
    sqe->len = iovecs.size();
    sqe->off = 0;
    sqe->user_data = 0; // TODO: 参考iocp
    sq_ring_.array[index] = index;
    tail = next_tail;
    if (*sq_ring_.tail != tail) {
        *sq_ring_.tail = tail;
        std::atomic_thread_fence(std::memory_order::acquire);
    }
}

void IOUring::handle_complete_task()
{
    unsigned head = *cq_ring_.head;
    do {
        std::atomic_thread_fence(std::memory_order::release);
        if (head == *cq_ring_.tail) {
            break;
        }
        auto cqe = &cq_ring_.cqes[head & *cq_ring_.ring_mask];
        // TODO: 从user_data中获得上下文信息
    } while (true);
}

uint8_t IOUring::action_to_opcode(IOUring::Action action)
{
    //似乎不支持accept之流
    switch (action) {
    case Action::Recv:
    case Action::Recvfrom:
        return IORING_OP_RECVMSG;
    case Action::Send:
    default:
        return IORING_OP_NOP;
    }
}

int IOUring::_io_uring_setup(unsigned entries, struct io_uring_params* p)
{
    return (int)syscall(__NR_io_uring_setup, entries, p);
}

int IOUring::_io_uring_enter(int ring_fd, unsigned int to_submit, unsigned int min_complete, unsigned int flags)
{
    return (int)syscall(__NR_io_uring_enter, ring_fd, to_submit, min_complete,
        flags, NULL, 0);
}

} // namespace net

} // namespace bco

#endif // ifdef __linux__
