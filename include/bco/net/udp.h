#pragma once
#include <bco/net/socket.h>
#include <bco/coroutine/task.h>
#include <bco/net/proactor/select.h>

namespace bco {

namespace net {

namespace detail {
#ifdef WIN32
template <typename T>
class RecvMsgFunc {
protected:
    void set_recvmsg_func(void*) {}
};
template <>
class RecvMsgFunc<Select> {
protected:
    void set_recvmsg_func(void* func) { recvmsg_func_ = func; }
    void* recvmsg_func_;
};
#else
template <typename T>
class RecvMsgFunc {
protected:
    void set_recvmsg_func(void*) {}
};
#endif // WIN32
} // namespace detail

template <SocketProactor P>
class UdpSocket : public detail::RecvMsgFunc<P> {
public:
    static std::tuple<UdpSocket, int> create(P* proactor, int family);

    UdpSocket() = default;
    UdpSocket(P* proactor, int family, int fd = -1);

    [[nodiscard]] Task<int> recv(bco::Buffer buffer);
    [[nodiscard]] Task<std::tuple<int, Address>> recvfrom(bco::Buffer buffer);
    int connect(const Address& addr);
    int send(bco::Buffer buffer);
    int sendto(bco::Buffer, const Address& addr);
    int bind(const Address& addr);
    void close();

private:
    P* proactor_;
    int family_;
    int socket_;
};

} // namespace net

} // namespace bco
