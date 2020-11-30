#include <array>
#include <string>
#include <memory>
#include <iostream>
#include <bco/bco.h>

class EchoServer : public std::enable_shared_from_this<EchoServer> {
public:
    EchoServer(bco::Context* ctx, uint32_t port)
        : ctx_(ctx), listen_port_(port)
    {
        //start();
    }

    void start()
    {
        ctx_->spawn(std::move([shared_this = shared_from_this()]() -> bco::Task<> {
            // bco::TcpSocket::TcpSocket(Context);
            auto [socket, error] = bco::TcpSocket::create(shared_this->ctx_->proactor());
            if (error < 0) {
                std::cerr << "Create socket failed with " << error << std::endl;
                co_return;
            }
            sockaddr_in server_addr;
            ::memset(&server_addr, 0, sizeof(server_addr));
            server_addr.sin_family = AF_INET;
            server_addr.sin_port = ::htons(shared_this->listen_port_);
            server_addr.sin_addr.S_un.S_addr = ::inet_addr("0.0.0.0");
            int ret = socket.bind(server_addr);
            if (ret != 0) {
                std::cerr << "bind: " << ret << std::endl;
            }
            ret = socket.listen();
            if (ret != 0) {
                std::cerr << "listen: " << ret << std::endl;
            }
            auto shared_that = shared_this;
            while (true) {
                bco::TcpSocket cli_sock = co_await socket.accept();
                shared_that->ctx_->spawn(std::bind(&EchoServer::serve, shared_that.get(), shared_that, cli_sock));
            }
        }));
    }

private:
    bco::Task<> serve(std::shared_ptr<EchoServer> shared_this, bco::TcpSocket sock)
    {
        std::array<uint8_t, 1024> data;
        while (true) {
            bco::Buffer buffer{data.data(), data.size()};
            int bytes_received = co_await sock.read(buffer);
            if (bytes_received == 0) {
                std::cout << "Stop serve one client\n";
                co_return;
            }
            auto recv_data = std::string((char*)buffer.data(), bytes_received);
            std::cout << "Received: " << recv_data;
            int bytes_sent = co_await sock.write(bco::Buffer { buffer.data(), static_cast<size_t>(bytes_received) });
            std::cout << "Sent: " << std::string((char*)buffer.data(), bytes_sent);
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
    auto server = std::make_shared<EchoServer>(&ctx, 30000);
    server->start();
    ctx.loop();
    return 0;
}