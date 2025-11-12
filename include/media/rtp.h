#pragma once
#include <cstdint>
#include <vector>
#include <cstring>
#include <string>
#include <arpa/inet.h>
#include "singleton.h"

#define RTCP_PACKET_TYPE_SR 200 // SR包，发送方报告
#define RTCP_PACKET_TYPE_RR 201 // RR包，接收方报告
#define RTCP_PACKET_TYPE_SDES 202   // SDES包，会话描述
#define RTCP_PACKET_TYPE_BYE 203    // BYE包，会话结束
#define RTCP_PACKET_TYPE_APP 204    // APP包，应用自定义

struct RTPHeader {
    // RTP头部的版本号，通常为2  
    uint8_t  version:2; // RTP版本，通常为2
    
    // padding标志，指示RTP包是否有填充
    // 如果设置为1，则表示RTP包的末尾有填充字节
    // 填充字节的数量由RTP包的最后一个字节指定
    // 填充字节通常用于确保RTP包的长度是4的倍数
    // 如果padding为0，则表示没有填充字节       
    uint8_t  padding:1; // 填充标志

    // extension标志，指示RTP包是否有扩展头部
    // 如果设置为1，则表示RTP包有扩展头部
    // 扩展头部通常用于携带额外的信息，例如时间戳、负载类型等
    // 扩展头部的格式由RTP协议定义
    // 如果extension为0，则表示没有扩展头部 
    uint8_t  extension:1;   // 扩展标志

    // CSRC计数，表示RTP包中包含的CSRC（贡献源）数量
    // CSRC用于标识RTP包的贡献源，例如多媒体会议中的参与者
    // CSRC计数的范围是0到15            
    uint8_t  csrc_len:4;        // CSRC计数

    // 标记位，通常用于指示RTP包的特定状态
    // 例如，在视频流中，标记位可以指示关键帧的开始
    // 在音频流中，标记位可以指示音频包的边界
    // 标记位的含义取决于RTP协议的具体实现
    // 如果marker为1，则表示RTP包具有特殊含义
    // 如果marker为0，则表示RTP包没有特殊含义   
    uint8_t  marker:1;  // 标记位
    
    // 负载类型占7位，这里用uint8_t存储，第8位用于标记位
    // 这样可以支持128种不同的负载类型
    // 负载类型通常由RTP协议定义，例如，96常用于动态负载类型，98常用于H.265视频
    // 负载类型用于指示RTP负载的数据格式
    uint8_t  payload_type:7;    // 负载类型
    
    // 序列号用于标识RTP包的顺序，通常是一个16位的无符号整数
    // 序列号在每个RTP会话中递增，通常从0开始
    // 这有助于接收端检测丢包和重排序
    // 序列号的范围是0到65535
    // 例如，如果序列号为1000，则表示这是第1000个           
    uint16_t seq_number;
    
    // 时间戳用于指示RTP包的时间信息，通常是一个32位的无符号整数
    // 时间戳的单位通常是采样率的倒数，例如，如果采样率为8000Hz，则时间戳的单位是125微秒
    // 时间戳用于同步RTP包的播放时间,范围是0到4294967295
    uint32_t timestamp;

    // SSRC（同步源标识符）用于唯一标识RTP会话中的源,是一个32位的无符号整数，通常由RTP会话的发送方随机生成
    // SSRC用于区分不同的RTP源，例如在多媒体会议中，SSRC可以用于区分不同的参与者,范围是0到4294967295
    uint32_t ssrc;

    RTPHeader(uint8_t pt, uint16_t seq, uint32_t ts, uint32_t ssrc_, bool marker_ = true)
        : version(2), padding(0), extension(0), csrc_len(0), marker(marker_ ? 1 : 0), payload_type(pt),
          seq_number(htons(seq)), timestamp(htonl(ts)), ssrc(htonl(ssrc_)) {}

    // 将RTP头部转换为字节数组
    void to_bytes(uint8_t* buf) const {
        buf[0] = (version << 6) | (padding << 5) | (extension << 4) | csrc_len;
        buf[1] = (marker << 7) | payload_type;
        std::memcpy(buf + 2, &seq_number, 2);
        std::memcpy(buf + 4, &timestamp, 4);
        std::memcpy(buf + 8, &ssrc, 4);
    }
};

struct RTPPacket {
    // RTP头部
    RTPHeader header;
    // 负载数据
    std::vector<uint8_t> payload;

    RTPPacket(RTPHeader h, std::vector<uint8_t> p) : header(h), payload(p) {}
    // 将RTP包转换为字节数组
    std::vector<uint8_t> to_bytes() const {
        std::vector<uint8_t> data(12 + payload.size());
        header.to_bytes(data.data());
        if (!payload.empty()) {
            std::memcpy(data.data() + 12, payload.data(), payload.size());
        }
        return data;
    }
};

// RTP包基类
class RtpPacket {
public:
    RTPPacket packet;
    RtpPacket(uint8_t payload_type, uint16_t seq, uint32_t ts, uint32_t ssrc, const uint8_t* payload, size_t payload_size)
        : packet(RTPHeader(payload_type, seq, ts, ssrc, true), std::vector<uint8_t>(payload, payload + payload_size)) {}
    virtual ~RtpPacket() = default;
};

// H.264 RTP包
class RtpPacketH264 : public RtpPacket {
public:
    RtpPacketH264(uint16_t seq, uint32_t ts, uint32_t ssrc, const uint8_t* nalu, size_t nalu_size)
        : RtpPacket(96, seq, ts, ssrc, nalu, nalu_size) {}
};

// H.265 RTP包
class RtpPacketH265 : public RtpPacket {
public:
    RtpPacketH265(uint16_t seq, uint32_t ts, uint32_t ssrc, const uint8_t* nalu, size_t nalu_size)
        : RtpPacket(98, seq, ts, ssrc, nalu, nalu_size) {}
};



struct RTCP_SR_PACKET {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIN__
    //  RTP/RTCP版本
    uint8_t version:2;
    // 填充位，用于填充数据，确保数据长度为4字节的倍数
    uint8_t padding:1;
    // 包类型，SR包的类型为200
    uint8_t rece_report_cnt:5;
#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    uint8_t rece_report_cnt:5;
    uint8_t padding:1;
    uint8_t version:2;
#endif
    // 包类型，SR包的类型为200
    uint8_t packet_type:8;
    // 包长，不含头
    uint16_t length;
    // 发送方ssrc
    uint32_t ssrc;
    // 发送方信息
    uint32_t ntp_sec; // NTP时间戳,高32位 （秒）
    uint32_t ntp_frac; // NTP时间戳，低32位（秒的小数部分）
    uint32_t rtp_ts; // RTP时间戳  
    uint32_t pkt_count; // 累计发送的RTP包数量
    uint32_t octet_count; // 累计发送的RTP包字节数
};

// 通用的RTCP SDES包
#define RTCP_SDES_PT 202    // 包类型，SDES包为202
#define RTCP_VERSION 2

// SDES 项类型
enum class RTCP_SdesItemType : uint8_t{
    RTCP_SDES_END   = 0,    // 结束项
    RTCP_SDES_CNAME = 1,    // 会话名称
    RTCP_SDES_NAME  = 2,    // 会话名称
    RTCP_SDES_EMAIL = 3,    // 会话名称
    RTCP_SDES_PHONE = 4,    // 会话名称
    RTCP_SDES_LOC   = 5,    // 会话名称
    RTCP_SDES_TOOL  = 6,    // 会话名称
    RTCP_SDES_NOTE  = 7,    // 会话名称
    RTCP_SDES_PRIV  = 8     // 会话名称
};

// SDES Item
struct RTCP_SdesItem{
    RTCP_SdesItemType type;  // 
    std::vector<uint8_t> data;

    RTCP_SdesItem(RTCP_SdesItemType t, const std::vector<uint8_t>& d) : type(t), data(d) {}
};

// SDES 条目（一个 SSRC + 多个 item）
struct RTCP_SdesChunk {
    uint32_t ssrc;  // 标识符，标识媒体流来源
    std::vector<RTCP_SdesItem> items;
};

// 0                   1                   2                   3
// 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                          SSRC/CSRC                            |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |  item type    |   length      | user and domain name ...
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |  item type    |   length      | data...
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |      ...      |               | ...
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |     END=0     |    padding...                                ...
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

class RTCPSdes : public Singleton<RTCPSdes> {
    friend class Singleton<RTCPSdes>;
public:

    std::vector<uint8_t> Pack() const;
    void Parse(const uint8_t* data, size_t size);
    // 添加一个SDES条目
    void AddItem(uint32_t ssrc, RTCP_SdesItemType type, const std::string& value);
    void RemoveItem(uint32_t ssrc, RTCP_SdesItemType type);
private:
    static void writeU32(std::vector<uint8_t>& buf, uint32_t val);
    static uint32_t readU32(const uint8_t* p);
    RTCP_SdesChunk& findOrCreateChunk(uint32_t ssrc);
    RTCP_SdesChunk* findChunk(uint32_t ssrc);
    const RTCP_SdesChunk* findChunk(uint32_t ssrc) const;
    std::vector<RTCP_SdesChunk> chunks_;
};
