#include <array>
#include <iostream>
#include <memory>
#include <string>

#include <bco.h>

using CurrProactor = bco::net::IOCP;

class EchoServer : public std::enable_shared_from_this<EchoServer> {
public:
    EchoServer(std::shared_ptr<bco::Context> ctx, uint16_t port)
        : ctx_(ctx)
        , listen_port_(port)
    {
        //start();
    }

    void start()
    {
        ctx_->spawn(std::move([shared_this = this->shared_from_this()]() -> bco::Routine {
            auto [socket, error] = bco::net::TcpSocket<CurrProactor>::create(shared_this->ctx_->get_proactor<CurrProactor>(), AF_INET);
            if (error < 0) {
                std::cerr << "Create socket failed with " << error << std::endl;
                co_return;
            }
            sockaddr_in server_addr {};
            server_addr.sin_family = AF_INET;
            server_addr.sin_port = ::htons(shared_this->listen_port_);
            server_addr.sin_addr = bco::to_ipv4("0.0.0.0");

            int ret = socket.bind(bco::net::Address { server_addr });
            if (ret != 0) {
                std::cerr << "bind: " << ret << std::endl;
            }
            ret = socket.listen(5);
            if (ret != 0) {
                std::cerr << "listen: " << ret << std::endl;
            }
            auto shared_that = shared_this;
            while (true) {
                auto [cli_sock, addr] = co_await socket.accept();
                shared_that->ctx_->spawn(std::bind(&EchoServer::serve, shared_that.get(), shared_that, cli_sock));
            }
        }));

        ctx_->start();
    }

private:
    bco::Routine serve(std::shared_ptr<EchoServer> shared_this, bco::net::TcpSocket<CurrProactor> sock)
    {
        bco::Buffer buffer(1024);
        while (true) {
            int bytes_received = co_await sock.recv(buffer);
            if (bytes_received == 0) {
                std::cout << "Stop serve one client\n";
                co_return;
            }
            const auto data = buffer.data();
            std::cout << "Received size:" << bytes_received << ", message:" << std::string((const char*)data[0].data(), bytes_received) << std::endl;
            int bytes_sent = co_await sock.send(buffer.subbuf(0, bytes_received));
            std::cout << "Sent size:" << bytes_received << ", message:" << std::string((const char*)data[0].data(), bytes_sent) << std::endl;
        }
    }

private:
    std::shared_ptr<bco::Context> ctx_;
    uint16_t listen_port_;
};

void init_winsock()
{
#ifdef _WIN32
    WSADATA wsdata;
    (void)WSAStartup(MAKEWORD(2, 2), &wsdata);
#endif
}

int main()
{
    init_winsock();
    auto ctx = std::make_shared<bco::Context>(std::make_unique<bco::SimpleExecutor>());
    auto socket_proactor = std::make_unique<CurrProactor>();
    socket_proactor->start(/*ctx->executor()*/);
    ctx->add_proactor(std::move(socket_proactor));
    auto server = std::make_shared<EchoServer>(ctx, uint16_t { 30000 });
    server->start();
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds { 1000 });
    }

    return 0;
}