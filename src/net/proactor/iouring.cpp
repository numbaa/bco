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
    //TODO: 扔到pending tasks里
    const auto slices = buff.data();
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
    sqe->fd = s;
    sqe->flags = 0;
    sqe->opcode = IORING_OP_RECVMSG;
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
    int ret = _io_uring_enter(fd_, 1, 0, 0);
    if (ret < 0) {
        return ret;
    }
}

int IOUring::recvfrom(int s, bco::Buffer buff, std::function<void(int, const sockaddr_storage&)> cb)
{
}

int IOUring::send(int s, bco::Buffer buff, std::function<void(int)> cb)
{
    //跟IOCP一样，不管是不是当前线程，都得先进队列
    //如果先尝试同步non-blocking发送，会多一次系统调用
}

int IOUring::send(int s, bco::Buffer buff)
{
}

int IOUring::sendto(int s, bco::Buffer buff, const sockaddr_storage& addr)
{
}

int IOUring::accept(int s, std::function<void(int)> cb)
{
}

int IOUring::connect(int s, const sockaddr_storage& addr, std::function<void(int)> cb)
{
}

int IOUring::connect(int s, const sockaddr_storage& addr)
{
}

std::vector<PriorityTask> IOUring::harvest_completed_tasks()
{
}

void IOUring::do_io()
{
    assert(executor_->is_current_executor());
    //
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
