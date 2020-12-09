#include <WinSock2.h>
#include <MSWSock.h>
#include <cassert>
#include <cstdint>
#include <chrono>
#include <vector>
#include <array>
#include <functional>
#include <thread>
#include <bco/net/proactor/iocp.h>
#include <bco/executor.h>
#include <bco/utils.h>
#include <iostream>

namespace bco {

namespace net {

constexpr size_t kAcceptBuffLen = sizeof(SOCKADDR_IN) * 2 + 32;

enum class OverlapAction {
    Unknown,
    Receive,
    Send,
    Accept,
    Connect,
};

#pragma warning(push)
#pragma warning(disable : 26495)
struct OverlapInfo {
    WSAOVERLAPPED overlapped;
    OverlapAction action;
    int sock;
    std::function<void(int)> cb;
};

struct AcceptOverlapInfo : OverlapInfo {
    std::array<uint8_t, kAcceptBuffLen> buff;
};
#pragma warning(pop)

IOCP::IOCP()
{
    this->complete_port_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 1);
}

int IOCP::create_fd()
{
    SOCKET fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return static_cast<int>(fd);
    ::CreateIoCompletionPort(reinterpret_cast<HANDLE>(fd), complete_port_, NULL, 0);
    return static_cast<int>(fd);
}

void IOCP::start()
{
    harvest_thread_ = std::move(std::thread { std::bind(&IOCP::iocp_loop, this) });
}

void IOCP::stop()
{
}

int IOCP::read(int s, std::span<std::byte> buff, std::function<void(int length)>&& cb)
{
    OverlapInfo* overlap_info = new OverlapInfo;
    overlap_info->action = OverlapAction::Receive;
    overlap_info->cb = std::move(cb);
    overlap_info->sock = s;
    ::SecureZeroMemory((PVOID)&overlap_info->overlapped, sizeof(WSAOVERLAPPED));
    WSABUF wsabuf {static_cast<ULONG>(buff.size()), reinterpret_cast<char*>(buff.data())};
    DWORD flags = 0;
    DWORD bytes_transferred;
    int ret = ::WSARecv(s, &wsabuf, 1, &bytes_transferred, &flags, &overlap_info->overlapped, nullptr);
    if (ret == SOCKET_ERROR) {
        int last_error = ::WSAGetLastError();
        return last_error == WSA_IO_PENDING ? 0 : last_error;
    }
    return 0;
    /*
    if (ret == 0) {
        delete overlap_info;
        return bytes_transferred;
    }
    if (ret == SOCKET_ERROR && ::WSAGetLastError() == WSA_IO_PENDING) {
        return 0;
    }
    */
    return -1;
}

int IOCP::write(int s, std::span<std::byte> buff, std::function<void(int length)>&& cb)
{
    OverlapInfo* overlap_info = new OverlapInfo;
    overlap_info->action = OverlapAction::Send;
    overlap_info->cb = std::move(cb);
    overlap_info->sock = s;
    ::SecureZeroMemory((PVOID)&overlap_info->overlapped, sizeof(WSAOVERLAPPED));
    WSABUF wsabuf {static_cast<ULONG>(buff.size()), reinterpret_cast<char*>(buff.data())};
    DWORD flags = 0;
    DWORD bytes_transferred;
    int ret = ::WSASend(s, &wsabuf, 1, &bytes_transferred, flags, &overlap_info->overlapped, nullptr);
    if (ret == SOCKET_ERROR) {
        int last_error = ::WSAGetLastError();
        return last_error == WSA_IO_PENDING ? 0 : last_error;
    }
    return 0;
    /*
    if (ret == 0) {
        delete overlap_info;
        return bytes_transferred;
    }
    if (ret == SOCKET_ERROR && ::WSAGetLastError() == WSA_IO_PENDING) {
        return 0;
    }
    return -1;
    */
}

int IOCP::accept(int s, std::function<void(int s)>&& cb)
{
    AcceptOverlapInfo* overlap_info = new AcceptOverlapInfo;
    overlap_info->action = OverlapAction::Accept;
    overlap_info->cb = std::move(cb);
    overlap_info->sock = static_cast<int>(::socket(AF_INET, SOCK_STREAM, 0));
    if (overlap_info->sock == INVALID_SOCKET) {
        delete overlap_info;
        return -1;
    }
    ::SecureZeroMemory((PVOID)&overlap_info->overlapped, sizeof(WSAOVERLAPPED));
    DWORD bytes;
    bool success = ::AcceptEx(s, overlap_info->sock, overlap_info->buff.data(), 0, sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, &bytes, &overlap_info->overlapped);
    if (success) {
        SOCKET sock = overlap_info->sock;
        delete overlap_info;
        //不让用nullptr...
        if (CreateIoCompletionPort(reinterpret_cast<HANDLE>(sock), complete_port_, NULL, 0) == complete_port_)
            return static_cast<int>(sock);
        else
            return -1;
    }
    if (WSAGetLastError() == WSA_IO_PENDING) {
        return 0;
    }
    return -1;
}

LPFN_CONNECTEX GetConnectEx(SOCKET so)
{
    GUID guid = WSAID_CONNECTEX;
    LPFN_CONNECTEX fnConnectEx = NULL;
    DWORD nBytes = 0;
    if (SOCKET_ERROR == WSAIoctl(so, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid), &fnConnectEx, sizeof(fnConnectEx), &nBytes, NULL, NULL)) {
        return NULL;
    }

    return fnConnectEx;
}

bool IOCP::connect(int s, sockaddr_in addr, std::function<void(int)>&& cb)
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
    bool success = ConnectEx(overlap_info->sock, (SOCKADDR*)&addr, sizeof(addr), nullptr, 0, &bytes_sent, &overlap_info->overlapped);
    if (success) {
        return success;
    }
    delete overlap_info;
    return false;
}


std::vector<PriorityTask> net::IOCP::harvest_completed_tasks()
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
        if (ret == 0 && overlapped == 0) {
            auto err = ::GetLastError();
            switch (err) {
            case WAIT_TIMEOUT:
                std::this_thread::yield();
                break;
            default:
                break;
            }
        } else if (ret == 0 && overlapped != 0) {
            auto err = ::GetLastError();
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
    OverlapInfo* overlap_info = reinterpret_cast<OverlapInfo*>(overlapped);
    switch (overlap_info->action) {
    case OverlapAction::Accept: {
        AcceptOverlapInfo* accept_info = reinterpret_cast<AcceptOverlapInfo*>(overlapped);
        if (accept_info->sock >= 0) {
            SOCKET handle = accept_info->sock;
            ::CreateIoCompletionPort(reinterpret_cast<HANDLE>(handle), complete_port_, NULL, 0);
        }
        {
            std::lock_guard lock { mtx_ };
            completed_tasks_.push_back(PriorityTask { 0, std::bind(overlap_info->cb, overlap_info->sock) });
        }
        delete accept_info;
        break;
    }
    case OverlapAction::Receive:
    case OverlapAction::Send: {
        std::lock_guard lock { mtx_ };
        completed_tasks_.push_back(PriorityTask { 0, std::bind(overlap_info->cb, bytes) });
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

}

}