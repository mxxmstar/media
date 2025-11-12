#include "boost_httpserver.h"
#include <boost/beast.hpp>
BoostHttpServer::BoostHttpServer(boost::asio::io_context& io, uint16_t port) : HttpServerBase(port), io_(io), 
    acceptor_(io, tcp::endpoint(tcp::v4(), port)), socket_(io) {

}

BoostHttpServer::~BoostHttpServer() {  
    stop(); 
}

void BoostHttpServer::start() {
    auto self = shared_from_this();
    acceptor_.async_accept(socket_, [self](boost::beast::error_code ec) {
        try{
            if (ec) {
                // 出错则放弃该连接
                self->start();
                return;
            }

            // 成功接受连接，创建HttpConnection处理请求
            // std::make_shared<HttpConnection>(std::move(self->socket_), self->handler_)->start();
            //继续监听
            self->start();
        }
        catch (const std::exception& e) {
            // std::cerr << "Error in BoostHttpServer: " << e.what() << std::endl;
            self->start(); // 继续监听
        }
    });
}

void BoostHttpServer::stop() {
    boost::system::error_code ec;
    acceptor_.close(ec); // 关闭监听器，停止接受新连接
    socket_.close(ec);   // 关闭当前socket连接
}
