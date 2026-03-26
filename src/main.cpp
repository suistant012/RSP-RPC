#include "server_network.hpp"

#include <cstdio>

//用于当前阶段的测试handler
class DummyNetworkHandler : public INetworkEventHandler {
public:
    void on_new_connection(Connection& conn) override {
        std::printf("[DummyHandler] new connection fd=%d\n", conn.connfd);
    }

    void on_connection_closed(int fd) override {
        std::printf("[DummyHandler] closed fd=%d\n", fd);
    }
    //测试回显，当读端条件成立后，将读端数据写到inbuf来触发回显，同时清理inbuf，后续会有专门的扩展处理
    //append到inbuf，最后回显输出通过handle_write里对同一fd的写实现，证明业务逻辑正确
    void on_readable(Connection& conn) override {
        std::printf("[DummyHandler] readable fd=%d\n", conn.connfd);
        conn.outbuf.append(conn.inbuf);
        conn.inbuf.clear();
    }

    void on_writable(Connection& conn) override {
        std::printf("[DummyHandler] writable fd=%d\n", conn.connfd);
    }
};

int main() {
    DummyNetworkHandler handler;
    ServerNetwork server(8888, &handler); // 绑定端口和IO处理器用于测试
    server.start();
    return 0;
}