#include <WinSock2.h>
#include <MSWSock.h>
#include <cassert>
#include <cstdint>
#include <chrono>
#include <vector>
#include <array>
#include <functional>
#include <bco/proactor/iocp.h>
#include <bco/executor.h>
#include <bco/buffer.h>
#include <iostream>

namespace bco {

constexpr size_t kAcceptBuffLen = sizeof(SOCKADDR_IN) * 2 + 32;

enum class OverlapAction {
    Unknown,
    Receive,
    Send,
    Accept,
    Connect,
};

struct OverlapInfo {
    WSAOVERLAPPED overlapped;
    OverlapAction action;
    int sock;
    std::function<void(int)> cb;
};

struct AcceptOverlapInfo : OverlapInfo {
    std::array<uint8_t, kAcceptBuffLen> buff;
};

static void handle_overlap_success(WSAOVERLAPPED* overlapped, int bytes, std::vector<std::function<void()>>& cbs)
{
    OverlapInfo* overlap_info = reinterpret_cast<OverlapInfo*>(overlapped);
    switch (overlap_info->action) {
    case OverlapAction::Accept: {
        AcceptOverlapInfo* accept_info = reinterpret_cast<AcceptOverlapInfo*>(overlapped);
        cbs.push_back(std::bind(overlap_info->cb, overlap_info->sock));
        delete accept_info;
        break;
    }
    case OverlapAction::Receive:
    case OverlapAction::Send:
        std::cout << "handle_overlap:" << (int)overlap_info->action << std::endl;
        cbs.push_back(std::bind(overlap_info->cb, bytes));
        delete overlap_info;
        break;
    default:
        assert(false);
    }
}

IOCP::IOCP()
{
    this->complete_port_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 1);
}

int IOCP::read(int s, std::span<std::byte> buff, std::function<void(int length)>&& cb)
{
    OverlapInfo* overlap_info = new OverlapInfo;
    overlap_info->action = OverlapAction::Receive;
    overlap_info->cb = std::move(cb);
    overlap_info->sock = s;
    ::SecureZeroMemory((PVOID)&overlap_info->overlapped, sizeof(WSAOVERLAPPED));
    WSABUF wsabuf {buff.size(), reinterpret_cast<char*>(buff.data())};
    DWORD flags = 0;
    DWORD bytes_transferred;
    int ret = ::WSARecv(s, &wsabuf, 1, &bytes_transferred, &flags, &overlap_info->overlapped, nullptr);
    std::cout << "Recv\n";
    if (ret == 0) {
        delete overlap_info;
        return bytes_transferred;
    }
    if (ret == SOCKET_ERROR && ::WSAGetLastError() == WSA_IO_PENDING) {
        return 0;
    }
    return -1;
}

int IOCP::write(int s, std::span<std::byte> buff, std::function<void(int length)>&& cb)
{
    OverlapInfo* overlap_info = new OverlapInfo;
    overlap_info->action = OverlapAction::Send;
    overlap_info->cb = std::move(cb);
    overlap_info->sock = s;
    ::SecureZeroMemory((PVOID)&overlap_info->overlapped, sizeof(WSAOVERLAPPED));
    WSABUF wsabuf {buff.size(), reinterpret_cast<char*>(buff.data())};
    DWORD flags = 0;
    DWORD bytes_transferred;
    int ret = ::WSASend(s, &wsabuf, 1, &bytes_transferred, flags, &overlap_info->overlapped, nullptr);
    std::cout << "Send\n";
    //return 0;
    if (ret == 0) {
        delete overlap_info;
        return bytes_transferred;
    }
    if (ret == SOCKET_ERROR && ::WSAGetLastError() == WSA_IO_PENDING) {
        return 0;
    }
    return -1;
}

int IOCP::accept(int s, std::function<void(int s)>&& cb)
{
    AcceptOverlapInfo* overlap_info = new AcceptOverlapInfo;
    overlap_info->action = OverlapAction::Accept;
    overlap_info->cb = std::move(cb);
    overlap_info->sock = ::WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
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
        if (CreateIoCompletionPort((HANDLE)sock, complete_port_, NULL, 0) == complete_port_)
            return sock;
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

bool IOCP::connect(sockaddr_in addr, std::function<void(int)>&& cb)
{
    OverlapInfo* overlap_info = new OverlapInfo;
    overlap_info->action = OverlapAction::Connect;
    overlap_info->cb = std::move(cb);
    overlap_info->sock = ::WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
    if (overlap_info->sock == INVALID_SOCKET) {
        delete overlap_info;
        return -1;
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

std::vector<std::function<void()>> IOCP::drain(uint32_t timeout_ms)
{
    DWORD bytes;
    LPOVERLAPPED overlapped;
    ULONG_PTR completion_key;
    DWORD remain_ms;
    if (timeout_ms == std::numeric_limits<uint32_t>::max()) {
        remain_ms = INFINITE;
    } else if (timeout_ms == 0) {
        remain_ms = 1;
    } else {
        remain_ms = timeout_ms;
    }
    std::vector<std::function<void()>> cbs;
    while (true) {
        auto start_time = std::chrono::high_resolution_clock().now();
        int ret = ::GetQueuedCompletionStatus(complete_port_, &bytes, &completion_key, &overlapped, remain_ms);
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
            break;
        } else if (ret != 0 && overlapped == 0) {
            assert(false);
        } else if (ret != 0 && overlapped != 0) {
            handle_overlap_success(overlapped, bytes, cbs);
            //break;
        } else {
            assert(false);
        }
        auto end_time = std::chrono::high_resolution_clock().now();
        auto used_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
        if (remain_ms <= used_ms)
            break;
        else
            remain_ms -= used_ms;
    }
    return std::move(cbs);
}

void IOCP::attach(int fd)
{
    ::CreateIoCompletionPort(reinterpret_cast<HANDLE>(fd), complete_port_, NULL, 0);
}

}
