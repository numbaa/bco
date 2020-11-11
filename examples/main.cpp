#include <array>
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
        ctx_->spawn([this]() -> bco::Task<> {
            // bco::TcpSocket::TcpSocket(Context);
            bco::TcpSocket socket{ctx_->proactor()};
            socket.bind();
            while (true) {
                bco::TcpSocket cli_sock = co_await socket.accept();
                ctx_->spawn(std::bind(&EchoServer::serve, this, cli_sock));
            }
        });
    }
    bco::Task<> serve(bco::TcpSocket sock)
    {
        std::array<uint8_t, 1024> data;
        while (true) {
            bco::Buffer buffer{data.data(), data.size()};
            int bytes_received = co_await sock.read(buffer);
            int bytes_sent = co_await sock.write(buffer);
        }
    }
private:
    bco::Context* ctx_;
    uint16_t listen_port_;
};

int main()
{
    bco::Context ctx;
    ctx.set_executor(std::make_unique<bco::Executor>());
    ctx.set_proactor(std::make_unique<bco::Proactor>());
    EchoServer server{&ctx, 30000};
    ctx.loop();
    return 0;
}