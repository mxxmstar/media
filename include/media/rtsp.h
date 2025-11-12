#pragma once
#include <string>
#include <cstdint>
#include <memory>
// rtsp://admin:admin12345@192.168.1.64:554/Streaming/Channels/101
// url: "rtsp://192.168.1.100:8554/live/stream1"
// ip: "192.168.1.100"
// port: 8554
// suffix: "live/stream1"
struct RtspUrlInfo {
    std::string url;    // 完整rtsp url
    std::string ip;     // 提取的服务器IP
    uint16_t port;      // 端口号，通常是554
    std::string suffix; // url的最后一段(资源名)
};

// rtsp 用户名认证信息
struct RtspUsrAuthInfo {
    typedef std::shared_ptr<RtspUsrAuthInfo> Ptr;
    bool has_auth_info_;
    std::string auth_domain;    // 认证域
    std::string username_;      // 用户名
    std::string password_;      // 密码
    std::string version_;       // 版本
    RtspUsrAuthInfo() : has_auth_info_(false), auth_domain(""), username_(""), password_(""), version_("") {}
    ~RtspUsrAuthInfo() {}
};
class Rtsp : public std::enable_shared_from_this<Rtsp> {
public:
    Rtsp();
    virtual ~Rtsp();
    bool ParseUrl(const std::string& url);
    bool ParseRtspUrl(const std::string& url);
protected:
    RtspUrlInfo urlInfo_;
    RtspUsrAuthInfo::Ptr authInfo_;
};
