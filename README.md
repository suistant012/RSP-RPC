# RSP-RPC

一个基于 **C++ / Linux / epoll** 的轻量级 RPC 项目。  
当前目标不是堆功能，而是**从网络层开始，把远程调用的核心链路一步一步搭出来**，做到结构清楚、职责清楚、流程能讲清、代码能跑通、调试能验证。

---

## 1. 项目说明

RPC（Remote Procedure Call，远程过程调用）的核心目标，是让客户端调用远端服务时，使用体验尽可能接近本地函数调用。

以本地函数调用为例：

int ret = Add(1, 2);

正常情况下，这个调用过程发生在：

- 同一个线程
- 同一个调用栈
- 同一个进程地址空间

函数执行结束后，控制流回到调用点，调用方得到返回值并继续执行。

而在 RPC 中，表面上客户端仍然像是在“调用一个函数”，但框架内部实际上需要解决这些问题：

- 客户端和服务端如何建立连接
- 请求和响应如何组织成协议消息
- 服务端如何接收、解析、分发请求
- 结果如何返回给客户端
- 客户端如何把返回结果重新交还给调用方

所以，**RSP-RPC 的本质**，就是把这条“远程调用链”包装成尽可能接近本地函数调用的体验。

---

## 2. 项目目标

本项目的目标不是做成一个功能堆砌的“大而全”框架，而是先完成一个**以学习为主要目的，结构完整、边界清晰、可以解释清楚核心原理**的 RPC 骨架。

当前设计重点：

- 服务端网络层自研：`socket + epoll + EventLoop + Connection + Buffer`
- 协议层和网络层分离
- 事件驱动，不让 EventLoop 直接知道业务函数类型
- Connection 只承接连接状态，不直接调用业务
- 后续为 Dispatcher、线程池、客户端调用封装留扩展空间
- 整体设计尽量避免非必要重构

---

## 3. 项目规划

项目整体分成 **8 个核心模块**。

---

### A. 双端共用模块

#### 1. Common

负责整个框架的基础设施，包括：

- 日志
- 错误码
- 公共工具
- 基础类型定义

之所以单独抽出来，是因为这些内容既不是服务端独有，也不是客户端独有，而是整个框架都会依赖的公共基础。

---

#### 2. Protocol

负责消息协议相关的全部内容，包括：

- Header
- `msg_type`
- `request_id`
- `body_len`
- `magic / version`
- `Request / Response / Message`
- `Codec（encode / decode）`

之所以把 Message 和 Codec 放进同一个模块，是因为在真实工程里：

- 协议格式
- 消息结构
- 编码解码

本来就是一条链，拆得过碎反而会让实现和理解都变得割裂。

---

### B. 服务端模块

#### 3. ServerNetwork

负责服务端网络层，包含：

- `listen socket`
- `conn socket`
- `epoll / EventLoop`
- `Connection`
- `inbuf / outbuf`
- `accept / read / write / close`

这一层是服务端网络通信的地基。  
Connection、Buffer、事件循环、Socket 行为都属于“连接管理”这一大类职责，因此统一放在这个模块中最合理。

---

#### 4. ServerDispatch

负责服务端消息分发，包含：

- `Dispatcher`
- `Router`
- `handler` 注册
- `method -> handler` 映射

这一层解决的问题是：

> 收到一个 Request 之后，应该把它交给谁处理。

它已经不再属于网络层，而是请求分发层。

---

#### 5. ServerRuntime

负责服务端运行时支撑，包含：

- `ThreadPool`
- 任务投递
- 后续可扩展的定时器 / 超时控制

这一层的职责不是通信，也不是协议，而是：

> 服务端请求被分发后，如何真正被调度执行。

---

#### 6. Server

负责服务端总控，包含：

- 服务端总入口
- 组装 `ServerNetwork / Protocol / ServerDispatch / ServerRuntime`
- 对外暴露 `start()`

这一层的存在意义，是把服务端内部几个核心模块真正组装成一个整体，而不是让外部直接拼装细节。

---

### C. 客户端模块

#### 7. ClientNetwork

负责客户端网络通信，包含：

- `connect`
- 客户端 socket
- 客户端事件循环（如果需要）
- `Connection / Buffer`
- 请求发送与响应接收

客户端同样需要网络层，因为它并不是“天然就拿到结果”，而是也要经历：

- 发请求字节流
- 收响应字节流
- 管理连接状态

只是客户端网络层通常会比服务端简单一些。

---

#### 8. ClientCaller

负责客户端调用封装，包含：

- `Requestor`
- `RpcCaller`
- `pending_map`
- `sync / future / callback`

这一层最终要解决的问题是：

> 怎么把远程调用包装成像本地函数调用一样的体验。

因此把这些内容合并成一个模块是最自然的。

---

## 4. 为什么这样规划

这个项目的规划不是按“知识点章节”拆的，而是按**职责边界**拆的。

这样设计的好处是：

### 1. 网络层、协议层、业务层边界清楚

- 网络层只管连接和字节流
- 协议层只管消息格式与编解码
- 分发层只管请求去向
- 运行时只管执行支撑

### 2. 有利于逐层推进

项目可以按下面顺序推进：

1. 先把服务端网络层做通
2. 再把协议层接上
3. 再做分发与运行时
4. 最后补客户端调用封装

### 3. 有利于后续扩展

后面无论你想补：

- 更完整的协议
- 更灵活的回调绑定
- 类型擦除
- 线程池增强
- 客户端同步 / 异步调用

都不会和前面的设计完全冲突。

---

## 5. 当前进度

### 已完成

- **ServerNetwork 第一版最小闭环**
    - `listenfd` 创建
    - `bind + listen`
    - `epoll_create1`
    - `EventLoop`
    - `accept()` 获取 `connfd`
    - `Connection` 状态对象
    - `inbuf / outbuf`
    - `handle_accept / handle_read / handle_write / close_connection`
    - 最小 echo 测试闭环
    - 第一轮 gdb 调试验证

### 当前状态

目前已经完成了整个项目的第一个核心模块：**ServerNetwork**。  
这一阶段最重要的价值不是“功能很多”，而是把下面这些事情真正打通了：

- 服务端如何成为监听端点
- epoll 如何统一管理连接事件
- 同一个 fd 在不同阶段为什么关注不同事件
- 为什么需要 Connection 和 inbuf/outbuf
- 非阻塞 socket + epoll + EventLoop 如何形成事件驱动闭环

### 下一步计划

接下来会继续推进：

1. Protocol
2. ServerDispatch
3. ServerRuntime
4. Server
5. ClientNetwork
6. ClientCaller
7. Common

并逐步补齐测试、调试记录和模块笔记。

---

## 6. 当前环境

开发环境固定为：

- **Windows + WSL Ubuntu**
- **VSCode Remote WSL**
- **CMake + Ninja**
- **Git**
- **gdb**

---

## 7. 当前原则

项目推进过程中坚持这几条原则：

- EventLoop 不直接知道业务函数类型
- Connection 不直接调用具体服务函数
- Protocol 不和业务参数类型绑死
- 先把骨架和边界做对，再逐步补工程细节
- 每个模块尽量形成：**设计图 + 代码 + 测试 + 调试记录 + 知识点总结**

---

## 8. 说明

这个项目当前还处于持续构建中。  
README 会随着模块推进不断更新。