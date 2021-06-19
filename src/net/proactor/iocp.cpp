#ifdef _WIN32
// clang-format off
#include <WinSock2.h>
#include <MSWSock.h>
// clang-format on
#include <cassert>
#include <cstdint>

#include <array>
#include <chrono>
#include <functional>
#include <thread>
#include <vector>

#include "../../common.h"
#include "bco/exception.h"
#include <bco/executor.h>
#include <bco/net/proactor/iocp.h>

namespace bco {

namespace net {

constexpr size_t kAcceptBuffLen = sizeof(SOCKADDR_IN) * 2 + 32;
constexpr ULONG_PTR kExitKey = 0xffeeddcc;

enum class OverlapAction {
    Unknown,
    Recv,
    Recvfrom,
    Send,
    Accept,
    Connect,
};

#pragma warning(push)
#pragma warning(disable : 26495)
struct OverlapInfo {
    WSAOVERLAPPED overlapped;
    OverlapAction action;
    SOCKET sock;
    std::function<void(int)> cb;
};

struct AcceptOverlapInfo : OverlapInfo {
    std::array<uint8_t, kAcceptBuffLen> buff;
    std::function<void(int, const sockaddr_storage&)> cb2;
};

struct RecvfromOverlapInfo : OverlapInfo {
    std::function<void(int, const sockaddr_storage&)> cb2;
    sockaddr_storage addr;
    int len = sizeof(addr);
};
#pragma warning(pop)

IOCP::IOCP()
{
    this->complete_port_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 1);
    if (this->complete_port_ == nullptr) {
        throw NetworkException {"Create IOCP failed"};
    }
}

IOCP::~IOCP()
{
    harvest_thread_.join();
    CloseHandle(complete_port_);
}

int IOCP::create(int domain, int type)
{
    SOCKET fd = ::socket(domain, type, 0);
    if (fd == INVALID_SOCKET)
        return -last_error();
    ::CreateIoCompletionPort(reinterpret_cast<HANDLE>(fd), complete_port_, NULL, 0);
    return static_cast<int>(fd);
}

void IOCP::start()
{
    harvest_thread_ = std::move(std::thread { std::bind(&IOCP::iocp_loop, this) });
}

void IOCP::stop()
{
    ::PostQueuedCompletionStatus(complete_port_, 0, kExitKey, nullptr);
}

int IOCP::recv(int s, bco::Buffer buff, std::function<void(int)> cb)
{
    OverlapInfo* overlap_info = new OverlapInfo;
    overlap_info->action = OverlapAction::Recv;
    overlap_info->cb = std::move(cb);
    overlap_info->sock = s;
    ::SecureZeroMemory((PVOID)&overlap_info->overlapped, sizeof(WSAOVERLAPPED));
    auto slices = buff.data();
    std::vector<WSABUF> wsabuf(slices.size());
    for (size_t i = 0; i < wsabuf.size(); i++) {
        wsabuf[i].buf = reinterpret_cast<CHAR*>(slices[i].data());
        wsabuf[i].len = static_cast<ULONG>(slices[i].size());
    }
    DWORD flags = 0;
    DWORD bytes_transferred;
    int ret = ::WSARecv(s, wsabuf.data(), static_cast<DWORD>(wsabuf.size()), &bytes_transferred, &flags, &overlap_info->overlapped, nullptr);
    if (ret == SOCKET_ERROR) {
        int last_error = ::WSAGetLastError();
        return last_error == WSA_IO_PENDING ? 0 : -last_error;
    }
    return 0;
}

int IOCP::recvfrom(int s, bco::Buffer buff, std::function<void(int, const sockaddr_storage&)> cb, void*)
{
    RecvfromOverlapInfo* overlap_info = new RecvfromOverlapInfo;
    overlap_info->action = OverlapAction::Recvfrom;
    overlap_info->cb2 = std::move(cb);
    overlap_info->sock = s;
    ::SecureZeroMemory((PVOID)&overlap_info->overlapped, sizeof(WSAOVERLAPPED));
    auto slices = buff.data();
    std::vector<WSABUF> wsabuf(slices.size());
    for (size_t i = 0; i < wsabuf.size(); i++) {
        wsabuf[i].buf = reinterpret_cast<CHAR*>(slices[i].data());
        wsabuf[i].len = static_cast<ULONG>(slices[i].size());
    }
    DWORD flags = 0;
    DWORD bytes_transferred;
    int ret = ::WSARecvFrom(s, wsabuf.data(), static_cast<DWORD>(wsabuf.size()), &bytes_transferred, &flags, reinterpret_cast<sockaddr*>(&overlap_info->addr), &overlap_info->len, &overlap_info->overlapped, nullptr);
    if (ret == SOCKET_ERROR) {
        int last_error = ::WSAGetLastError();
        return last_error == WSA_IO_PENDING ? 0 : -last_error;
    }
    return 0;
}

int IOCP::send(int s, bco::Buffer buff, std::function<void(int length)> cb)
{
    OverlapInfo* overlap_info = new OverlapInfo;
    overlap_info->action = OverlapAction::Send;
    overlap_info->cb = std::move(cb);
    overlap_info->sock = s;
    ::SecureZeroMemory((PVOID)&overlap_info->overlapped, sizeof(WSAOVERLAPPED));
    auto slices = buff.data();
    std::vector<WSABUF> wsabuf(slices.size());
    for (size_t i = 0; i < wsabuf.size(); i++) {
        wsabuf[i].buf = reinterpret_cast<CHAR*>(slices[i].data());
        wsabuf[i].len = static_cast<ULONG>(slices[i].size());
    }
    DWORD flags = 0;
    DWORD bytes_transferred;
    int ret = ::WSASend(s, wsabuf.data(), static_cast<DWORD>(wsabuf.size()), &bytes_transferred, flags, &overlap_info->overlapped, nullptr);
    if (ret == SOCKET_ERROR) {
        int last_error = ::WSAGetLastError();
        return last_error == WSA_IO_PENDING ? 0 : -last_error;
    }
    return 0;
}

int IOCP::send(int s, bco::Buffer buff)
{
    int bytes = syscall_sendv(s, buff);
    if (bytes == -1)
        return -::WSAGetLastError();
    else
        return bytes;
}

int IOCP::sendto(int s, bco::Buffer buff, const sockaddr_storage& addr, void*)
{
    int bytes = syscall_sendmsg(s, buff, addr);
    if (bytes == -1)
        return -::WSAGetLastError();
    else
        return bytes;
}

//only ipv4 now
int IOCP::accept(int s, std::function<void(int, const sockaddr_storage&)> cb)
{
    AcceptOverlapInfo* overlap_info = new AcceptOverlapInfo;
    overlap_info->action = OverlapAction::Accept;
    overlap_info->cb2 = std::move(cb);
    overlap_info->sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (overlap_info->sock == INVALID_SOCKET) {
        delete overlap_info;
        return -1;
    }
    ::SecureZeroMemory((PVOID)&overlap_info->overlapped, sizeof(WSAOVERLAPPED));
    DWORD bytes;
    bool success = ::AcceptEx(s, overlap_info->sock, overlap_info->buff.data(), 0, sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, &bytes, &overlap_info->overlapped);
    int last_error = ::WSAGetLastError();
    if (!success && last_error != WSA_IO_PENDING) {
        return -last_error;
    }
    return 0;
}

LPFN_CONNECTEX GetConnectEx(SOCKET so)
{
    GUID guid = WSAID_CONNECTEX;
    LPFN_CONNECTEX fnConnectEx = nullptr;
    DWORD nBytes = 0;
    if (SOCKET_ERROR == WSAIoctl(so, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid), &fnConnectEx, sizeof(fnConnectEx), &nBytes, NULL, NULL)) {
        return nullptr;
    }

    return fnConnectEx;
}

int IOCP::connect(int s, const sockaddr_storage& addr, std::function<void(int)> cb)
{
    OverlapInfo* overlap_info = new OverlapInfo;
    overlap_info->action = OverlapAction::Connect;
    overlap_info->cb = std::move(cb);
    overlap_info->sock = s;
    if (overlap_info->sock == INVALID_SOCKET) {
        delete overlap_info;
        return false;
    }
    ::SecureZeroMemory((PVOID)&overlap_info->overlapped, sizeof(WSAOVERLAPPED));
    DWORD bytes_sent;
    LPFN_CONNECTEX ConnectEx = GetConnectEx(overlap_info->sock);
    if (ConnectEx == nullptr) {
        return -::WSAGetLastError();
    }
    bool success = ConnectEx(overlap_info->sock, (SOCKADDR*)&addr, sizeof(addr), nullptr, 0, &bytes_sent, &overlap_info->overlapped);
    if (success || ::WSAGetLastError() == ERROR_IO_PENDING) {
        return 0;
    } else {
        delete overlap_info;
        return -::WSAGetLastError();
    }
}

int IOCP::connect(int s, const sockaddr_storage& addr)
{
    int ret = ::connect(s, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
    if (ret == -1) {
        return -::WSAGetLastError();
    } else {
        return 0;
    }
}

std::vector<PriorityTask> net::IOCP::harvest()
{
    std::lock_guard lock { mtx_ };
    return std::move(completed_tasks_);
}

void IOCP::iocp_loop()
{
    while (true) {
        DWORD bytes;
        LPOVERLAPPED overlapped;
        ULONG_PTR completion_key;
        DWORD timeout = next_timeout();
        int ret = ::GetQueuedCompletionStatus(complete_port_, &bytes, &completion_key, &overlapped, timeout);
        if (bytes == 0 && completion_key == kExitKey && overlapped == nullptr) {
            return;
        }
        if (ret == 0 && overlapped == 0) {
            auto err = ::WSAGetLastError();
            switch (err) {
            case WAIT_TIMEOUT:
                std::this_thread::yield();
                break;
            default:
                break;
            }
        } else if (ret == 0 && overlapped != 0) {
            auto err = ::WSAGetLastError();
            (void)err;
            //TODO: handler error
        } else if (ret != 0 && overlapped == 0) {
            assert(false);
        } else if (ret != 0 && overlapped != 0) {
            handle_overlap_success(overlapped, bytes);
        } else {
            assert(false);
        }
    }
}

void IOCP::handle_overlap_success(WSAOVERLAPPED* overlapped, int bytes)
{
    // TODO: error handling
    OverlapInfo* overlap_info = reinterpret_cast<OverlapInfo*>(overlapped);
    switch (overlap_info->action) {
    case OverlapAction::Accept: {
        AcceptOverlapInfo* accept_info = reinterpret_cast<AcceptOverlapInfo*>(overlapped);
        sockaddr_storage addr {};
        if (accept_info->sock >= 0) {
            SOCKET handle = accept_info->sock;
            ::CreateIoCompletionPort(reinterpret_cast<HANDLE>(handle), complete_port_, NULL, 0);
            sockaddr* local = nullptr;
            sockaddr* remote = nullptr;
            int local_len, remote_len;
            GetAcceptExSockaddrs(accept_info->buff.data(), 0, sizeof(sockaddr_storage) + 16, sizeof(sockaddr_storage) + 16, &local, &local_len, &remote, &remote_len);
            if (remote != nullptr) {
                addr = *reinterpret_cast<sockaddr_storage*>(remote);
            }
        }
        {
            std::lock_guard lock { mtx_ };
            completed_tasks_.push_back(PriorityTask { Priority::Medium, std::bind(accept_info->cb2, static_cast<int>(overlap_info->sock), addr) });
        }
        delete accept_info;
        break;
    }
    case OverlapAction::Recv:
    case OverlapAction::Send: {
        std::lock_guard lock { mtx_ };
        completed_tasks_.push_back(PriorityTask { Priority::Medium, std::bind(overlap_info->cb, bytes) });
    }
        delete overlap_info;
        break;
    case OverlapAction::Recvfrom: {
        RecvfromOverlapInfo* rf_info = reinterpret_cast<RecvfromOverlapInfo*>(overlapped);
        {
            std::lock_guard lock { mtx_ };
            completed_tasks_.push_back(PriorityTask { Priority::Medium, std::bind(rf_info->cb2, bytes, rf_info->addr) });
        }
        delete rf_info;
        break;
    }
    case OverlapAction::Connect: {
        std::lock_guard lock { mtx_ };
        completed_tasks_.push_back(PriorityTask { Priority::Medium, std::bind(overlap_info->cb, bytes) });
    }
        delete overlap_info;
        break;
    default:
        assert(false);
    }
}

DWORD IOCP::next_timeout()
{
    return 10;
}

} // namespace net

} // namespace bco

#endif // ifdef _WIN32