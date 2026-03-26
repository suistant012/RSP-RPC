#include "server_network.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>

// servernetwork默认初始化各个类成员变量
ServerNetwork::ServerNetwork(uint16_t port,INetworkEventHandler* handler)
    : listenfd_(-1),epfd_(-1),port_(port),handler_(handler) {}

// 设置fd(socket)为非阻塞
void ServerNetwork::set_noblocking(int fd){
    int flags = :: fcntl(fd, F_GETFL, 0);
    ::fcntl(fd,F_SETFL,flags | O_NONBLOCK); //保留原有状态的基础上，新增非阻塞状态
}

void ServerNetwork::init_listen_socket(){
    listenfd_ = ::socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in addr{}; //IPV4地址结构体，创建初始化listenfd_监听地址
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port_);

    ::bind(listenfd_,reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    ::listen(listenfd_,SOMAXCONN);
    set_noblocking(listenfd_);
    
    std::printf("[ServerNetwork] listenfd=%d port=%u\n", listenfd_, port_);
}

void ServerNetwork::init_epoll() {
    epfd_ = ::epoll_create1(0);

    epoll_event ev{}; //epoll事件描述结构体
    ev.events = EPOLLIN; //关注可读事件
    ev.data.fd = listenfd_; //关注事件发生时带回的数据

    ::epoll_ctl(epfd_, EPOLL_CTL_ADD, listenfd_, &ev); //将listenfd交给epoll实例epfd管理

    std::printf("[ServerNetwork] epfd=%d\n", epfd_);
}

void ServerNetwork::update_epoll_events(int fd,uint32_t events){
    epoll_event ev{};
    ev.events = events;
    ev.data.fd = fd;
    ::epoll_ctl(epfd_, EPOLL_CTL_MOD, fd, &ev); //更改epoll监听状态
}

void ServerNetwork::handle_accept() {
    sockaddr_in cli{};
    socklen_t len = sizeof(cli);
    //listenfd_通过accept分发已建立连接到connfd，用connfd标志这个建立的连接
    int connfd = ::accept(listenfd_, reinterpret_cast<sockaddr*>(&cli), &len);
    if(connfd < 0) return;
    set_noblocking(connfd);

    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = connfd;
 
    ::epoll_ctl(epfd_,EPOLL_CTL_ADD, connfd, &ev);

    connections_[connfd] = Connection{connfd,"",""}; //在连接表中注册connfd

    if(handler_ != nullptr){
        handler_->on_new_connection(connections_[connfd]);
    }
    //测试连接建立结果
    char ip[INET_ADDRSTRLEN];
    ::inet_ntop(AF_INET, &cli.sin_addr, ip, sizeof(ip));
    std::printf("[ServerNetwork] accepted %s:%u fd=%d\n",ip , ntohs(cli.sin_port), connfd);
}

void ServerNetwork::handle_read(int fd) {
    auto it = connections_.find(fd);
    if(it == connections_.end()) return;

    char buf[4096]; //读缓冲区
    //当前版本EPOLL为LT模式，为了测试业务闭环，后续会改成ET
    int ret = ::read(fd,buf,sizeof(buf));
    //ret>0表示读到了数据
    if(ret > 0) {
        it->second.inbuf.append(buf,ret); //将一次read读到的内容并入inbuf
        if(handler_ != nullptr){
        handler_->on_readable(it->second); //调用其他层执行函数
    }
    if(!it->second.outbuf.empty()) {
        //在读处理时被测试回显写入outbuf
        //修改epoll监听fd状态为EPOLLIN+EPOLLOUT
        //表示当前不能放弃可能没读完的inbuf，也不能遗漏处理outbuf的写端事件
        update_epoll_events(fd, EPOLLIN | EPOLLOUT);
    }
        return ;
    }
    if(ret == 0) {
        close_connection(fd);
        return ;
    }
    //在handle_read中表示无数据可读但不必关闭连接
    if(errno == EAGAIN || errno ==EWOULDBLOCK){
        return ;
    }

    close_connection(fd);
}

void ServerNetwork::handle_write(int fd) {
    auto it = connections_.find(fd);
    if(it == connections_.end()) return;
    
    if(it->second.outbuf.empty()){
        //当outbuf空，将epoll原本监听fd的EPOLLIN+EPOLLOUT状态改为只监听EPOLLIN
        //这里和下面的同样处理逻辑缺一不可
        //这里是为了防止epoll_wait表面通知有outbuf有数据，但因为某些原因执行到这里outbuf数据消失
        //如果没有这一步，ret因为没有能写的值为0
        //这样就会导致空写不断进行
        update_epoll_events(fd, EPOLLIN);
        return ;
    }

    int ret = ::write(fd, it->second.outbuf.data(), it->second.outbuf.size());
    
    if(ret > 0) {
        // 完成一次写后，从outbuf取出已经完成写的ret个字符
        it->second.outbuf.erase(0,ret);

        if (it->second.outbuf.empty()) {
            //这里和上面的逻辑区分，表示恰好写完的情况
            update_epoll_events(fd, EPOLLIN);

            if (handler_ != nullptr) {
                handler_->on_writable(it->second);
            }
        }
        return ;
    }

    if(ret < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        return ;
    }

    close_connection(fd);
}

void ServerNetwork::close_connection(int fd){
    //从epoll管理对象中删除注册的fd（socket）
    ::epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr);
    ::close(fd);

    connections_.erase(fd);

    if(handler_ != nullptr){
        handler_->on_connection_closed(fd);
    }
}

void ServerNetwork::event_loop(){
    std::printf("[ServerNetwork] event_loop start\n");
    while(true){
        epoll_event events[1024];
        //epollwait统一等待fd，接下来的循环统一分发
        int n = ::epoll_wait(epfd_, events, 1024, -1);

        for(int i = 0;i < n;i++) {
            int fd = events[i].data.fd;
            uint32_t ev = events[i].events;
            //fd是负责分发连接的listenfd_
            if(fd == listenfd_){
                handle_accept();
                continue;
            }
            //fd异常或关闭
            if(ev & (EPOLLHUP | EPOLLERR)){
                close_connection(fd);
                continue;
            }
            //fd读端事件触发
            if(ev & EPOLLIN){
                handle_read(fd);
            }
            //fd写端事件触发
            if(ev & EPOLLOUT){
                handle_write(fd);
            }
        }
    }
}

void ServerNetwork::start() {
    std::printf("[ServerNetwork] start()\n");
    init_listen_socket();
    init_epoll();
    event_loop();
}