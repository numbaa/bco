#include <array>
#include <string>
#include <iostream>
#include <memory>
#include <bco/bco.h>
#include <bco/proactor.h>
#include <bco/context.h>
#include <bco/net/socket.h>
#include <bco/net/proactor/iocp.h>
#include <bco/net/proactor/select.h>
#include <bco/executor.h>
#include <bco/executor/simple_executor.h>


template <typename P> requires bco::net::SocketProactor<P>
class EchoServer : public std::enable_shared_from_this<EchoServer<P>> {
public:
    EchoServer(bco::Context* ctx, uint32_t port)
        : ctx_(ctx), listen_port_(port)
    {
        //start();
    }

    void start()
    {
        ctx_->spawn(std::move([shared_this = this->shared_from_this()]() -> bco::Task<> {
            // todo change nullptr to proactor
            auto [socket, error] = bco::net::TcpSocket<P>::create(nullptr);
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
                bco::net::TcpSocket<P> cli_sock = co_await socket.accept();
                shared_that->ctx_->spawn(std::bind(&EchoServer::serve, shared_that.get(), shared_that, cli_sock));
            }
        }));
    }

private:
    bco::Task<> serve(std::shared_ptr<EchoServer> shared_this, bco::net::TcpSocket<P> sock)
    {
        std::array<uint8_t, 1024> data;
        while (true) {
            std::span<std::byte> buffer((std::byte*)data.data(), data.size());
            int bytes_received = co_await sock.read(buffer);
            if (bytes_received == 0) {
                std::cout << "Stop serve one client\n";
                co_return;
            }
            std::cout << "Received: " << std::string((char*)buffer.data(), bytes_received);
            int bytes_sent = co_await sock.write(std::span<std::byte> { buffer.data(), static_cast<size_t>(bytes_received) });
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
    (void)WSAStartup(MAKEWORD(2, 2), &wsdata);
}

int main()
{
    init_winsock();
    auto iocp = std::make_unique<bco::net::IOCP>();
    //iocp->start();
    auto se = std::make_unique<bco::net::Select>();
    se->start();
    auto executor = std::make_unique<bco::SimpleExecutor>();
    bco::Context ctx { std::move(executor) };
    ctx.add_proactor(std::move(se));
    auto server = std::make_shared<EchoServer<bco::net::Select>>(&ctx, 30000);
    server->start();
    ctx.start();
    return 0;
}