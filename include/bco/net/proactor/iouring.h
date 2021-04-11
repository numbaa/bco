#pragma once
#ifdef __linux__
#include <linux/io_uring.h>
#include <netinet/in.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>

#include <bco/buffer.h>
#include <bco/executor.h>
#include <bco/net/address.h>
#include <bco/proactor.h>

namespace bco {

namespace net {

class IOUring {
public:
    struct SQPoll {
        std::chrono::milliseconds idle_time;
    };

    struct Params {
        std::optional<SQPoll> sq_poll;
        std::optional<uint32_t> queue_depth;
    };

    class GetterSetter {
    public:
        IOUring* proactor() { return iouring_.get(); }
        IOUring* socket_proactor() { return iouring_.get(); }
        void set_socket_proactor(std::unique_ptr<IOUring>&& s)
        {
            iouring_ = std::move(s);
        }

    private:
        std::unique_ptr<IOUring> iouring_;
    };

private:
    struct SqRing {
        void* mmap_ptr;
        size_t ring_size;
        unsigned* head;
        unsigned* tail;
        unsigned* ring_mask;
        unsigned* ring_entries;
        unsigned* flags;
        unsigned* array;
    };
    struct CqRing {
        void* mmap_ptr;
        size_t ring_size;
        unsigned* head;
        unsigned* tail;
        unsigned* ring_mask;
        unsigned* ring_entries;
        ::io_uring_cqe* cqes;
    };
    enum class Action : uint32_t {
        Recv,
        Send,
        Recvfrom,
        Accept,
        Connect,
    };
    struct UringTask {
        int fd;
        Action action;
        bco::Buffer buff;
        std::function<void(int)> cb;
        std::function<void(int, const sockaddr_storage&)> cb2;
        UringTask() = default;
        UringTask(int _fd, Action _action, bco::Buffer _buff, std::function<void(int)> _cb);
        UringTask(int _fd, Action _action, bco::Buffer _buff, std::function<void(int, const sockaddr_storage&)> _cb);
    };

public:
    IOUring(const Params& params);
    ~IOUring();

    void start(ExecutorInterface* executor);
    void stop();

    int create(int domain, int type);
    int bind(int s, const sockaddr_storage& addr);
    int listen(int s, int backlog);

    int recv(int s, bco::Buffer buff, std::function<void(int)> cb);

    int recvfrom(int s, bco::Buffer buff, std::function<void(int, const sockaddr_storage&)> cb);

    int send(int s, bco::Buffer buff, std::function<void(int)> cb);
    int send(int s, bco::Buffer buff);

    int sendto(int s, bco::Buffer buff, const sockaddr_storage& addr);

    int accept(int s, std::function<void(int)> cb);

    int connect(int s, const sockaddr_storage& addr, std::function<void(int)> cb);
    int connect(int s, const sockaddr_storage& addr);

    std::vector<PriorityTask> harvest_completed_tasks();

private:
    void do_io();
    std::queue<UringTask> get_pending_tasks();
    void submit_tasks(std::queue<UringTask>& tasks);
    void submit_one_task(const UringTask& task);
    void handle_complete_task();
    uint8_t action_to_opcode(Action action);
    void setup_io_uring(const Params& params);
    int _io_uring_setup(unsigned entries, struct io_uring_params* p);
    int _io_uring_enter(int ring_fd, unsigned int to_submit, unsigned int min_complete, unsigned int flags);

private:
    int fd_;
    ExecutorInterface* executor_;
    std::queue<UringTask> pending_tasks_;
    std::mutex mutex_;
    size_t sq_entries_ {};
    ::io_uring_sqe* sqes_ { nullptr };
    SqRing sq_ring_ {};
    CqRing cq_ring_ {};
};

} // namespace net

} // namespace bco

#endif // ifdef __linux__
