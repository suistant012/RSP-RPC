#pragma once

#include <cstdint>
#include <unordered_map>

#include "connection.hpp"
#include "network_event_handler.hpp"

class ServerNetwork {
public:
    explicit ServerNetwork(uint16_t port,INetworkEventHandler* handler = nullptr); // 禁止隐式转换构造
    void start(); // 入口启动函数
private:
    int listenfd_; // 服务端监听fd
    int epfd_; // epoll实例的fd句柄
    uint16_t port_;
    std::unordered_map<int,Connection> connections_; //连接表，

    INetworkEventHandler* handler_; //IO事件处理器 

    /****初始化 + 主循环模块声明*****/
    void init_listen_socket(); // 初始化监听fd(socket)
    void init_epoll(); // epoll实例初始化
    void update_epoll_events(int fd, uint32_t events); //更新epoll监听状态
    void event_loop(); // Eventloop方法声明
    
    /****IO事件处理方法声明 ****/
    void handle_accept(); // 处理新连接
    void handle_read(int fd); // 处理连接读
    void handle_write(int fd); // 处理连接写
    void close_connection(int fd); // 处理连接关闭

    /**** 工具函数声明 ****/
    void set_noblocking(int fd); //设置fd(socket)为非阻塞
};