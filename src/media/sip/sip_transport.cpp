#include "sip_transport.h"
#include "stringhelper.h"
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

/// @brief SIP响应报文
// SIP/2.0 100 Trying
// Via: SIP/2.0/UDP pc33.atlanta.com;branch=z9hG4bK776asdhds
// From: Alice <sip:alice@atlanta.com>;tag=1928301774
// To: Bob <sip:bob@biloxi.com>
// Call-ID: a84b4c76e66710@pc33.atlanta.com
// CSeq: 314159 INVITE
// Content-Length: 0

SipMessage SipMessage::Parse(const std::string& data) {
    SipMessage msg;
    std::istringstream iss(data);
    std::string line;

    //解析首行
    // INVITE sip:34020000001320000001@3402000000 SIP/2.0
    if(!std::getline(iss, line)) {
        throw std::runtime_error("SipMessage::Parse: SIP message is empty!");
    }

    line = StringHelper::trim(line);
    if(line.empty()) {
        throw std::runtime_error("SipMessage::Parse: SIP message first line is empty!");
    }

    if(line.find("SIP/") == 0) {
        // 响应报文 SIP/2.0 100 Trying
        size_t sp1 = line.find(' ');
        size_t sp2 = line.find(' ', sp1 + 1);
        if(sp1 == std::string::npos || sp2 == std::string::npos) {
            throw std::runtime_error("SipMessage::Parse: Invalid SIP response start line");
        }
        msg.version_ = line.substr(0, sp1);
        try {
            msg.status_code_ = std::stoi(line.substr(sp1 + 1, sp2 - sp1 - 1));
        } catch (const std::exception& e) {
            throw std::runtime_error("SipMessage::Parse: Invalid SIP response start line");
        }
        msg.reason_ = StringHelper::trim(line.substr(sp2 + 1));
    }


    bool first_line = true;
    bool in_body = false;

    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();

        if (first_line) {
            first_line = false;
            std::istringstream ls(line);
            std::string first_word;
            ls >> first_word;

            if (first_word.rfind("SIP/", 0) == 0) {
                msg.version_ = first_word;
                ls >> msg.status_code_;
                std::getline(ls, msg.reason_);
                if (!msg.reason_.empty() && msg.reason_[0] == ' ')
                    msg.reason_.erase(0, 1);
            } else {
                msg.method_ = first_word;
                ls >> msg.uri_ >> msg.version_;
            }
            continue;
        }

        if (!in_body) {
            if (line.empty()) {
                in_body = true;
                continue;
            }
            auto pos = line.find(':');
            if (pos != std::string::npos) {
                std::string key = line.substr(0, pos);
                std::string value = line.substr(pos + 1);
                if (!value.empty() && value[0] == ' ')
                    value.erase(0, 1);
                msg.headers_[key] = value;
            }
        } else {
            msg.body_ += line + "\n";
        }
    }
    return msg;
}

std::string SipMessage::ToString() const {
    std::ostringstream oss;
    if (!method_.empty()) {
        oss << method_ << " " << uri_ << " " << version_ << "\r\n";
    } else {
        oss << version_ << " " << status_code_ << " " << reason_ << "\r\n";
    }

    for (auto& [k, v] : headers_) {
        oss << k << ": " << v << "\r\n";
    }
    oss << "Content-Length: " << body_.size() << "\r\n";
    oss << "\r\n";
    oss << body_;
    return oss.str();
}
