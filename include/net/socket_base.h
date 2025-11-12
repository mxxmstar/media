#pragma once
#include <string>
#include <cstdint>

namespace net {

enum class SocketType {
    TCP,
    UDP
};
enum class SocketProtocol {
    IPv4,
    IPv6
};
enum class SocketState {
    Closed,
    Listening,
    Connected,
    Error
};

//socket抽象基类
class Socket {
public:
    virtual ~Socket() = default;
    /// @brief 连接到指定地址和端口
    /// @param address 目标地址
    /// @param port 目标端口
    /// @return 成功返回true，失败返回false
    virtual bool connect(const std::string& address, uint16_t port) = 0;

    /// @brief 发送数据
    /// @param buf 数据缓冲区
    /// @param len 数据长度
    /// @return 成功发送的字节数，失败返回-1
    virtual size_t send(const void* buf, size_t len) = 0;
    
    /// @brief 接收数据
    /// @param buf 接收缓冲区
    /// @param len 缓冲区长度
    /// @return 成功接收的字节数，失败返回-1
    virtual size_t recv(void* buf, size_t len) = 0;
    
    /// @brief 关闭套接字
    virtual void close() = 0;
};    

}
