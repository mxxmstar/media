#pragma once

/**
 * @brief SIP transport层
 * @details UDP/TCP/TLS 的收发与粘包
 */

// INVITE sip:34020000001320000001@3402000000 SIP/2.0
// Via: SIP/2.0/TCP 192.168.1.10:5060;branch=z9hG4bK-123456
// From: <sip:34020000002000000001@3402000000>;tag=1234
// To: <sip:34020000001320000001@3402000000>
// Call-ID: 987654321@192.168.1.10
// CSeq: 1 INVITE
// Content-Length: 129

// v=0
// o=34020000002000000001 0 0 IN IP4 192.168.1.10
// s=Play
// c=IN IP4 192.168.1.10
// t=0 0
// m=video 30000 RTP/AVP 96
// a=rtpmap:96 PS/90000


#include <string>
#include <map>
#include <sstream>
#include <functional>
#include <memory>
#include <vector>
#include "net/asio_socket.h"

// @brief SIP消息
class SipMessage {
public:
    struct RemoteInfo {
        std::string protocol;   // "UDP" "TCP" "TLS" "WebSocket"
        std::string ip;
        uint16_t port = 0;

        RemoteInfo() = default;
        RemoteInfo(const std::string& ptcl, const std::string& addr, uint16_t pt)
            : protocol(ptcl), ip(addr), port(pt) {}
    };

    SipMessage() = default;
    /// @brief 从字符串解析SIP消息
    /// @param  msg 字符串
    /// @return 解析后的SIP消息
    static SipMessage Parse(const std::string&);

    /// @brief 将SipMessage转换为字符串
    /// @return 字符串
    std::string ToString() const;
        // ---- setters ----
    void set_method(const std::string& method) { method_ = method; }
    void set_uri(const std::string& uri) { uri_ = uri; }
    void set_version(const std::string& version) { version_ = version; }
    void set_status_code(int code) { status_code_ = code; }
    void set_reason(const std::string& reason) { reason_ = reason; }
    void set_body(const std::string& body) { body_ = body; }
    void set_header(const std::string& key, const std::string& value) { headers_[key] = value; }
    void set_remote(const RemoteInfo& remote) { remote_ = remote; }

    // ---- getters ----
    const std::string& method() const { return method_; }
    const std::string& uri() const { return uri_; }
    const std::string& version() const { return version_; }
    int status_code() const { return status_code_; }
    const std::string& reason() const { return reason_; }
    const std::string& body() const { return body_; }
    const std::map<std::string, std::string>& headers() const { return headers_; }
    const RemoteInfo& remote() const { return remote_; }

private:
    std::string method_; // 请求方法 INVITE/REGISTER/ACK/BYE/...
    std::string uri_;    // 请求行中的Request-URI
    std::string version_;    // SIP/2.0
    int status_code_ = 0;    //如果是响应报文
    std::string reason_;     //响应原因
    std::map<std::string, std::string> headers_; // 头字段
    std::string body_; // 消息体(SDP/XML/JSON)
    RemoteInfo remote_; // 传输协议
};

// @brief SIP传输层
class SipTransport {
public:
    using Ptr = std::shared_ptr<SipTransport>;
    using MessageHandler = std::function<void(const SipMessage&)>;

    virtual ~SipTransport() = default;

    // 启动传输
    virtual void Start() = 0;

    // 停止传输
    virtual void Stop() = 0;

    // 发送SIP消息
    virtual void Send(const SipMessage& msg) = 0;

    // 设置收到消息的回调
    void SetMsgHandler(MessageHandler handler) {
        // 移动赋值
        handler_ = std::move(handler);
    }

    void SetTapHandler(MessageHandler handler) {
        tap_handlers_.push_back(std::move(handler));
    }

protected:
    // 内部触发消息分发
    void dispatch_message(const SipMessage& msg) {
        // 先通知 tap 监听
        for (auto& h : tap_handlers_) {
            if (h) h(msg);
        }
        // 再交给主 handler
        if (handler_) {
            handler_(msg);
        }
    }

    MessageHandler handler_;    // 给到Transaction层的回调函数
    std::vector<MessageHandler> tap_handlers_;
};

/// @brief UDP SIP传输层
class UdpSipTransport : public SipTransport{
public:
    UdpSipTransport(ASIO::IoContext& ctx, const std::string& listen_ip, uint16_t port)
        : socket_(ctx, ASIO::UdpEndpoint(boost::asio::ip::make_address(listen_ip), port)) {}

    void Start() override;

    void Stop() override;

    void Send(const SipMessage& msg) override;
    
private:

    std::string serialize(const SipMessage& msg);
    std::string parseSipMsg();
    ASIO::UdpSocket socket_;
    ASIO::UdpEndpoint endpoint_;
};

