#include "rtsp.h"

Rtsp::Rtsp() : authInfo_(std::make_shared<RtspUsrAuthInfo>()) {

}

// url: "rtsp://192.168.1.100:8554/live/stream1"
bool Rtsp::ParseRtspUrl(const std::string& url) {
    // 判断头是否为“rtsp://”
    if(url.rfind("rtsp://", 0) != 0) {
        return false;
    }
    std::string addr = url.substr(7);
    auto slash_pos = addr.find('/');
    if(slash_pos == std::string::npos) {
        return false;
    }

    std::string host_post = addr.substr(0, slash_pos);
    urlInfo_.suffix = addr.substr(slash_pos + 1);
    urlInfo_.url = url;

    auto port_pos = host_post.find(':');
    if(port_pos == std::string::npos) {
        urlInfo_.ip = host_post;
        urlInfo_.port = 554;
    } else {
        urlInfo_.ip = host_post.substr(0, port_pos);
        try {
            urlInfo_.port = std::stoi(host_post.substr(port_pos + 1));
        } catch (...) {
            // TODO: 添加日志
            return false;
        }
        
    }
    return true;
}