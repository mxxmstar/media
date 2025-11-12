#pragma once
#include <string>
#include <functional>

// HTTP请求结构体（可根据需要扩展）
struct HttpRequest {
    std::string method;
    std::string uri;
    std::string body;
    // ... 可扩展 header、参数等
};

// HTTP响应结构体（可根据需要扩展）
struct HttpResponse {
    int status_code = 200;
    std::string body;
    // ... 可扩展 header 等
};

// HTTP服务抽象基类
class HttpServerBase {
public:
    explicit HttpServerBase(uint16_t port) : port_(port) {}
    virtual ~HttpServerBase() = default;
    // 启动服务，监听端口
    virtual void start() = 0;
    // 停止服务
    virtual void stop() = 0;
protected:
    uint16_t port_;
};
