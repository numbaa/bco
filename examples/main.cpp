#include <array>
#include <string>
#include <iostream>
#include <memory>
#include <bco/bco.h>
#include <bco/proactor.h>
#include <bco/proactor/iocp.h>

template <typename P> requires bco::Proactor<P>
class EchoServer {
public:
    EchoServer(bco::Context<P>* ctx, uint32_t port)
        : ctx_(ctx), listen_port_(port)
    {
        start();
    }

private:
    void start()
    {
        ctx_->spawn(std::move([this]() -> bco::Task<> {
            // bco::TcpSocket::TcpSocket(Context);
            auto [socket, error] = bco::TcpSocket<P>::create(ctx_->proactor());
            if (error < 0) {
                std::cerr << "Create socket failed with " << error << std::endl;
                co_return;
            }
            sockaddr_in server_addr;
            ::memset(&server_addr, 0, sizeof(server_addr));
            server_addr.sin_family = AF_INET;
            server_addr.sin_port = ::htons(listen_port_);
            server_addr.sin_addr.S_un.S_addr = ::inet_addr("0.0.0.0");
            int ret = socket.bind(server_addr);
            if (ret != 0) {
                std::cerr << "bind: " << ret << std::endl;
            }
            ret = socket.listen();
            if (ret != 0) {
                std::cerr << "listen: " << ret << std::endl;
            }
            while (true) {
                auto xthis = this;
                bco::TcpSocket<P> cli_sock = co_await socket.accept();
                xthis->ctx_->spawn(std::bind(&EchoServer::serve, xthis, cli_sock));
            }
        }));
    }
    bco::Task<> serve(bco::TcpSocket<P> sock)
    {
        std::array<uint8_t, 1024> data;
        while (true) {
            std::span<std::byte> buffer((std::byte*)data.data(), data.size());
            int bytes_received = co_await sock.read(buffer);
            std::cout << "Received: " << std::string((char*)buffer.data(), bytes_received);
            int bytes_sent = co_await sock.write(std::span<std::byte> { buffer.data(), static_cast<size_t>(bytes_received) });
            std::cout << "Sent" << std::endl;
        }
    }
private:
    bco::Context<P>* ctx_;
    uint16_t listen_port_;
};

void init_winsock()
{
    WSADATA wsdata;
    WSAStartup(MAKEWORD(2, 2), &wsdata);
}

template <typename T> requires bco::Proactor<T>
void func_use_proactor(T obj)
{
}

int main()
{
    init_winsock();
    auto iocp = std::make_unique<bco::IOCP>();
    auto executor = std::make_unique<bco::Executor>();
    bco::Context ctx { std::move(iocp), std::move(executor) };
    EchoServer server{&ctx, 30000};
    ctx.loop();
    //bco::IOCP xx;
    //func_use_proactor(xx);
    return 0;
}