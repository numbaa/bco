#include <array>
#include <string>
#include <iostream>
#include <bco/bco.h>

class EchoServer {
public:
    EchoServer(bco::Context* ctx, uint32_t port)
        : ctx_(ctx), listen_port_(port)
    {
        start();
    }

private:
    void start()
    {
        ctx_->spawn(std::move([this]() -> bco::Task<> {
            // bco::TcpSocket::TcpSocket(Context);
            auto [socket, error] = bco::TcpSocket::create(ctx_->proactor());
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
                bco::TcpSocket cli_sock = co_await socket.accept();
                xthis->ctx_->spawn(std::bind(&EchoServer::serve, xthis, cli_sock));
            }
        }));
    }
    bco::Task<> serve(bco::TcpSocket sock)
    {
        std::array<uint8_t, 1024> data;
        while (true) {
            bco::Buffer buffer{data.data(), data.size()};
            int bytes_received = co_await sock.read(buffer);
            std::cout << "Received: " << std::string((char*)buffer.data(), bytes_received);
            int bytes_sent = co_await sock.write(bco::Buffer { buffer.data(), static_cast<size_t>(bytes_received) });
        }
    }
private:
    bco::Context* ctx_;
    uint16_t listen_port_;
};

void init_winsock()
{
    WSADATA wsdata;
    WSAStartup(MAKEWORD(2, 2), &wsdata);
}

int main()
{
    init_winsock();
    bco::Context ctx;
    ctx.set_executor(std::make_unique<bco::Executor>());
    ctx.set_proactor(std::make_unique<bco::Proactor>());
    EchoServer server{&ctx, 30000};
    ctx.loop();
    return 0;
}