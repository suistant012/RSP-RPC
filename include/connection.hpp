#pragma once
#include<string>

struct Connection {
    int connfd; // 连接fd
    std::string inbuf; // 连接读缓冲区
    std::string outbuf; // 连接写缓冲区
};