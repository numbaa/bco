#ifdef __linux__
#include <linux/version.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <cassert>
#include <cstring>
#include <functional>
#include <regex>

#include "../../common.h"
#include <bco/exception.h>
#include <bco/net/proactor/iouring.h>

namespace bco {

namespace net {

namespace {
enum class Opcode : uint8_t {
    NOP,
    READV,
    WRITEV,
    FSYNC,
    READ_FIXED,
    WRITE_FIXED,
    POLL_ADD,
    POLL_REMOVE,
    SYNC_FILE_RANGE,
    SENDMSG,
    RECVMSG,
    TIMEOUT,
    TIMEOUT_REMOVE,
    ACCEPT,
    ASYNC_CANCEL,
    LINK_TIMEOUT,
    CONNECT,
    FALLOCATE,
    OPENAT,
    CLOSE,
    FILES_UPDATE,
    STATX,
    READ,
    WRITE,
    FADVISE,
    MADVISE,
    SEND,
    RECV,
    OPENAT2,
    EPOLL_CTL,
    SPLICE,
    PROVIDE_BUFFERS,
    REMOVE_BUFFERS,
    TEE,
    SHUTDOWN,
    RENAMEAT,
    UNLINKAT,

    /* this goes last, obviously */
    LAST,

};
} // namespace

IOUring::IOUring(const Params& params)
{
    verify_kernel_version();
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

void IOUring::verify_kernel_version()
{
    ::utsname name {};
    int ret = ::uname(&name);
    if (ret < 0) {
        throw NetworkException { "get kernel version failed" };
    }
    std::string str = name.release;
    std::regex pattern { "^([0-9]+?)\\.([0-9]+?)\\.([0-9]+?)" };
    std::smatch sm;
    if (!std::regex_match(str, sm, pattern)) {
        throw NetworkException { "get kernel version failed" };
    }
    auto major = std::stoi(sm[1].str());
    auto minor = std::stoi(sm[2].str());
    if (KERNEL_VERSION(major, minor, 0) < KERNEL_VERSION(5, 5, 0)) {
        throw NetworkException { "kernel version lower than 5.5" };
    }
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

int IOUring::recv(int s, bco::Buffer buff, std::function<void(int)> cb)
{
    uint64_t id = lastest_task_id_.fetch_add(1);
    std::lock_guard lock { mutex_ };
    pending_tasks_.emplace(id, UringTask { id, s, Action::Recv, buff, cb });
}

int IOUring::recvfrom(int s, bco::Buffer buff, std::function<void(int, const sockaddr_storage&)> cb, void*)
{
    uint64_t id = lastest_task_id_.fetch_add(1);
    std::lock_guard lock { mutex_ };
    pending_tasks_.emplace(id, UringTask { id, s, Action::Recvfrom, buff, cb, true });
}

int IOUring::send(int s, bco::Buffer buff, std::function<void(int)> cb)
{
    uint64_t id = lastest_task_id_.fetch_add(1);
    std::lock_guard lock { mutex_ };
    pending_tasks_.emplace(id, UringTask { id, s, Action::Send, buff, cb });
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
int IOUring::sendto(int s, bco::Buffer buff, const sockaddr_storage& addr, void*)
{
    int bytes = syscall_sendmsg(s, buff, addr);
    if (bytes == -1)
        return -last_error();
    else
        return bytes;
}

int IOUring::accept(int s, std::function<void(int, const sockaddr_storage&)> cb)
{
    uint64_t id = lastest_task_id_.fetch_add(1);
    std::lock_guard lock { mutex_ };
    pending_tasks_.emplace(id, UringTask { id, s, Action::Accept, bco::Buffer {}, cb, true });
}

int IOUring::connect(int s, const sockaddr_storage& addr, std::function<void(int)> cb)
{
    uint64_t id = lastest_task_id_.fetch_add(1);
    std::lock_guard lock { mutex_ };
    pending_tasks_.emplace(id, UringTask { id, s, Action::Connect, bco::Buffer {}, cb });
    pending_tasks_[id].addr = addr;
}

int IOUring::connect(int s, const sockaddr_storage& addr)
{
    int error = ::connect(s, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
    if (error == -1)
        return -last_error();
    else
        return 0;
}

std::vector<PriorityTask> IOUring::harvest()
{
    std::lock_guard lock { mutex_ };
    return std::move(completed_task_);
}

void IOUring::do_io()
{
    assert(executor_->is_current_executor());

    handle_complete_tasks();
    auto pending_tasks = get_pending_tasks();
    submit_tasks(pending_tasks);
    handle_complete_tasks(); //有必要么
    using namespace std::chrono_literals;
    executor_->post_delay(1ms, bco::PriorityTask { .priority = 1, .task = std::bind(&IOUring::do_io, this) });
}

std::map<uint64_t, IOUring::UringTask> IOUring::get_pending_tasks()
{
    std::lock_guard lock { mutex_ };
    return std::move(pending_tasks_);
}

void IOUring::submit_tasks(std::map<uint64_t, IOUring::UringTask>& tasks)
{
    size_t ops = 0;
    for (auto& it : tasks) {
        //FIXME:如果sqe不够了，把剩下的tasks扔回pending tasks
        submit_one_task(it.second);
        ops++;
    }
    int ret = _io_uring_enter(fd_, ops, 0, 0);
    if (ret < 0) {
        // TODO: error handling;
    }
    flying_tasks_.insert(std::move(tasks.extract(tasks.begin())));
}

void IOUring::submit_one_task(IOUring::UringTask& task)
{
    switch (task.action) {
    case Action::Recv:
    case Action::Recvfrom:
    case Action::Send:
        submit_rw(task);
        break;
    case Action::Connect:
        submit_connect(task);
        break;
    case Action::Accept:
        submit_accept(task);
        break;
    default:
        break;
    }
}

void IOUring::submit_rw(IOUring::UringTask& task)
{
    const auto slices = task.buff.data();
    task.iovecs.resize(slices.size());
    for (size_t i = 0; i < task.iovecs.size(); i++) {
        task.iovecs[i] = ::iovec { slices[i].data(), slices[i].size() };
    }
    void* addr = nullptr;
    if (task.addr.has_value()) {
        addr = &task.addr.value();
    }
    ::msghdr hdr {
        .msg_name = addr == nullptr ? nullptr : reinterpret_cast<sockaddr*>(addr),
        .msg_namelen = addr == nullptr ? 0 : sizeof(sockaddr_storage),
        .msg_iov = task.iovecs.data(),
        .msg_iovlen = task.iovecs.size(),
        .msg_control = nullptr,
        .msg_controllen = 0,
        .msg_flags = 0,
    };
    submit_sqe(task.fd, action_to_opcode(task.action), &hdr, sizeof(::msghdr), task.id);
}

void IOUring::submit_connect(IOUring::UringTask& task)
{
    submit_sqe(task.fd, action_to_opcode(task.action), &task.addr.value(), sizeof(sockaddr_storage), task.id);
}

void IOUring::submit_accept(IOUring::UringTask& task)
{
    submit_sqe(task.fd, action_to_opcode(task.action), &task.addr.value(), sizeof(sockaddr_storage), task.id);
}

void IOUring::submit_sqe(int32_t fd, uint8_t opcode, void* addr, uint64_t len, uint64_t user_data)
{
    unsigned next_tail = *sq_ring_.tail;
    unsigned tail = next_tail;
    next_tail++;
    std::atomic_thread_fence(std::memory_order::release);
    unsigned index = tail & *sq_ring_.ring_mask;
    ::io_uring_sqe* sqe = sqes_ + index;
    sqe->fd = fd;
    sqe->flags = 0;
    sqe->opcode = opcode;
    sqe->addr = reinterpret_cast<decltype(sqe->addr)>(addr);
    sqe->len = len;
    sqe->off = 0;
    sqe->user_data = user_data;
    sq_ring_.array[index] = index;
    tail = next_tail;
    if (*sq_ring_.tail != tail) {
        *sq_ring_.tail = tail;
        std::atomic_thread_fence(std::memory_order::acquire);
    }
}

void IOUring::handle_complete_tasks()
{
    unsigned head = *cq_ring_.head;
    do {
        std::atomic_thread_fence(std::memory_order::release);
        if (head == *cq_ring_.tail) {
            break;
        }
        auto cqe = &cq_ring_.cqes[head & *cq_ring_.ring_mask];
        //成功、失败
        handle_complete_task(cqe->user_data, cqe);
        head++;
    } while (true);
}

void IOUring::handle_complete_task(uint64_t id, const io_uring_cqe* cqe)
{
    auto task = flying_tasks_.find(id);
    if (task == flying_tasks_.end()) {
        return;
    }
    switch (task->second.action) {
    case Action::Recv:
    case Action::Send:
    case Action::Connect:
        completed_task_.push_back(PriorityTask { 0, std::bind(task->second.cb, cqe->res) });
        break;
    case Action::Recvfrom:
    case Action::Accept:
        completed_task_.push_back(PriorityTask { 0, std::bind(task->second.cb2, cqe->res, task->second.addr.value()) });
        break;
    default:
        break;
    }
}

uint8_t IOUring::action_to_opcode(IOUring::Action action)
{
    //似乎不支持accept之流
    constexpr auto to_opcode = [](IOUring::Action act) -> Opcode {
        switch (act) {
        case Action::Recv:
        case Action::Recvfrom:
            return Opcode::RECVMSG;
        case Action::Send:
            return Opcode::SENDMSG;
        case Action::Connect:
            return Opcode::CONNECT;
        case Action::Accept:
            return Opcode::ACCEPT;
        default:
            return Opcode::NOP;
        }
    };
    return static_cast<uint8_t>(to_opcode(action));
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
