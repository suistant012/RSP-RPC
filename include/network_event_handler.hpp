#pragma once

#include "connection.hpp"
//ServerNetwork层只负责处理网络问题，这些fd业务逻辑交给其他层
class INetworkEventHandler {
public:
    virtual ~INetworkEventHandler() = default;

    virtual void on_new_connection(Connection& conn) = 0;
    virtual void on_connection_closed(int fd) = 0;
    virtual void on_readable(Connection& conn) = 0;
    virtual void on_writable(Connection& conn) = 0;
};