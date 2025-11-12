#pragma once
#include "net/httpserver_base.h"
#include <boost/asio.hpp>
#include <thread>
#include <atomic>

using tcp = boost::asio::ip::tcp;

// 继承std::enable_shared_from_this用于安全获取自身的shared_ptr
class BoostHttpServer : public HttpServerBase, protected std::enable_shared_from_this<BoostHttpServer> {
public:
    /// 构造函数，传入Boost.Asio的IO上下文和监听端口
    explicit BoostHttpServer(boost::asio::io_context& io, uint16_t port);
    ~BoostHttpServer() override;

    void start() override;
    void stop() override;

private:
    // Boost.Asio IO上下文
    boost::asio::io_context& io_;
    // TCP接受器
    boost::asio::ip::tcp::acceptor acceptor_;
    // TCP套接字
    boost::asio::ip::tcp::socket socket_;

};
