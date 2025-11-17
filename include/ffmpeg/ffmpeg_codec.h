#pragma once
extern "C" { 
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
}

#include "ffmpeg_avformat.h"

#include <stdexcept>
#include <string>
#include <functional>
#include <atomic>
namespace FFmpeg { 

struct VideoCodecParams {
    VideoCodecParams(std::string c, int w, int h, double f, AVPixelFormat p, int b) 
        : codec_name(c), width(w), height(h), fps(f), pix_fmt(p), bit_rate(b) {}
    std::string codec_name; // 编码器名称
    int width = -1;         // 输出宽度（-1表示使用输入宽度）
    int height = -1;        // 输出高度（-1表示使用输入高度）
    double fps = -1.0;      // 输出帧率（-1.0表示使用输入帧率）
    AVPixelFormat pix_fmt = AV_PIX_FMT_NONE; // 输出像素格式（AV_PIX_FMT_NONE表示使用输入格式）
    int bit_rate = -1;      // 输出比特率（-1表示使用输入比特率）
};


/// @brief 压缩后的 视频/音频 编码包
class Packet {
public:
    /// @brief 构造函数
    explicit Packet();

    /// @brief 析构函数
    ~Packet();
    
    /// @brief 禁止拷贝
    Packet(const Packet&) = delete;
    Packet& operator=(const Packet&) = delete;

    /// @brief 移动构造函数
    Packet(Packet&& other) noexcept;

    /// @brief 移动赋值运算符
    Packet& operator=(Packet&& other) noexcept;
    
    /// @brief 清空数据
    void unref() noexcept;

    /// @brief 获取内部的AVPacket指针
    ::AVPacket* get() noexcept;

    /// @brief 获取内部的AVPacket指针
    ::AVPacket* raw() noexcept;

    /// @brief 获取内部的AVPacket指针（常量版本）
    const ::AVPacket* get() const noexcept;

    /// @brief 获取内部的AVPacket指针（常量版本）
    const ::AVPacket* raw() const noexcept;

private:
    ::AVPacket* pkt_ = nullptr;
};

/// @brief 原始视频帧/音频帧
class Frame {
public:
    /// @brief 自定义 deleter, 释放AVFrame
    struct AVFrameDeleter {
        void operator()(AVFrame* frame) noexcept;
    };
    using UniqueAVFrame = std::unique_ptr<AVFrame, AVFrameDeleter>;
public: 
    /// @brief 构造函数，创建AVFrame,不申请帧缓冲区
    Frame();

    /// @brief 析构函数，释放空间
    ~Frame() noexcept;

    /// @brief 移动构造函数
    Frame(Frame&& other) noexcept;

    /// @brief 移动赋值运算符
    Frame& operator=(Frame&& other) noexcept;

    /// @brief 禁止拷贝
    Frame(const Frame&) = delete;

    /// @brief 禁止赋值
    Frame& operator=(const Frame&) = delete;

    /// @brief 申请视频帧缓冲区
    void AllocVideoBuffer(int width, int height, AVPixelFormat format, int align = 0);

    /// @brief 申请音频帧缓冲区
    void AllocAudioBuffer(int sample_rate, int nb_samples, int channels, AVSampleFormat format, int align = 0);

    /// @brief 获取内部的AVFrame指针
    ::AVFrame* get() noexcept;

    /// @brief 获取AVFrame指针（常量版本）
    const ::AVFrame* get() const noexcept;

    /// @brief 获取内部的AVFrame指针
    ::AVFrame* raw() noexcept;
    
    /// @brief 获取AVFrame指针（常量版本）
    const ::AVFrame* raw() const noexcept;

    /// @brief 显示释放内部的AVFrame指针
    void free() noexcept;
    
    /// @brief 移交AVFrame对象的所有权
    AVFrame* take_ownership() noexcept;

    /// @brief 重置内部的AVFrame指针
    void reset(AVFrame* frame) noexcept;

    /// @brief 浅拷贝数据到新的 AVFrame，仅用于非 hw frame -> sw frame 直接拷贝场景
    void clone_from(const Frame& frame);

    /// @brief 类似于智能指针的 bool 转换
    explicit operator bool() const noexcept; 

    /// @brief 类似于智能指针的->转换
    AVFrame* operator->() noexcept;
    const AVFrame* operator->() const noexcept;

    /// @brief 类似于智能指针的*转换
    AVFrame& operator*() noexcept;
    const AVFrame& operator*() const noexcept;

    /// @brief 工厂函数，创建AVFrame指针，并申请视频帧缓冲区
    static AVFrame* CreateVideoFrame(int width, int height, AVPixelFormat format, int align = 0);

    /// @brief 工厂函数，创建AVFrame指针，并申请音频帧缓冲区
    static AVFrame* CreateAudioFrame(int sample_rate, int nb_samples, int channels, AVSampleFormat format, int align = 0);

    /// @brief 释放AVFrame指针
    static void FreeFrame(AVFrame* frame) noexcept;

private:
    ::AVFrame* frame_ = nullptr;
};

/// @brief 编/解码器基类上下文
class CodecContext { 
public:
    /// @brief 默认构造函数
    /// @note 内部codec_ctx_和codec_均为nullptr，需要调用InitFromID()函数或InitFromName()函数初始化
    /// @note 定义默认构造函数，让子类不需要在初始化列表中进行初始化
    CodecContext() = default;

    /// @brief 构造函数 创建编/解码器上下文
    /// @param codec_id 编/解码器ID
    /// @param is_decoder 是否为解码器 true:解码器 false:编码器
    CodecContext(AVCodecID codec_id, bool is_decoder = true);

    /// @brief 构造函数 创建编/解码器上下文
    /// @param codec_name 编/解码器名称
    /// @param is_decoder 是否为解码器 true:解码器 false:编码器
    CodecContext(const std::string& codec_name, bool is_decoder = true);

    /// @brief 构造函数 创建编/解码器上下文
    /// @param codec 编/解码器
    /// @param is_decoder 是否为解码器 true:解码器 false:编码器
    CodecContext(const AVCodec* codec, bool is_decoder = true);

    /// @brief 构造函数 创建编/解码器上下文，并从AVStream中复制参数
    /// @param stream AVStream*流
    /// @param is_decoder 是否为解码器 true:解码器 false:编码器
    CodecContext(AVStream* stream, bool is_decoder = true);

    /// @brief 析构函数
    ~CodecContext();

    /// @brief 禁止拷贝
    CodecContext(const CodecContext&) = delete;
    CodecContext& operator=(const CodecContext&) = delete;

    /// @brief 移动构造函数
    CodecContext(CodecContext&& other) noexcept;
    
    /// @brief 移动赋值运算符
    CodecContext& operator=(CodecContext&& other) noexcept;

    /// @brief 工厂函数，创建编/解码器上下文
    /// @param codec_id 编/解码器ID
    /// @param is_decoder 是否为解码器 true:解码器 false:编码器
    static AVCodecContext* CreateCodecCtxFromID(AVCodecID codec_id, bool is_decoder = true, AVCodec** codec = nullptr);

    /// @brief 工厂函数，创建编/解码器上下文
    /// @param codec_name 编/解码器名称
    /// @param is_decoder 是否为解码器 true:解码器 false:编码器
    /// @note 需要再调用SetCodecParams()函数设置编/解码参数后才能使用
    static AVCodecContext* CreateCodecCtxFromName(const std::string& codec_name, bool is_decoder = true, AVCodec** codec = nullptr);

    /// @brief 工厂函数，创建编/解码器上下文
    /// @param codec 编/解码器
    /// @param is_decoder 是否为解码器 true:解码器 false:编码器
    /// @note 需要再调用SetCodecParams()函数设置编/解码参数后才能使用
    static AVCodecContext* CreateCodecCtxFromCodec(const AVCodec* codec, bool is_decoder = true);

    /// @brief 工厂函数，创建编/解码器上下文
    /// @param stream AVStream*流
    /// @param is_decoder 是否为解码器 true:解码器 false:编码器
    /// @note 解码时会通过AVStream获取解码参数，编码时需要手动设置编码参数
    static AVCodecContext* CreateCodecCtxFromStream(AVStream* stream, bool is_decoder = true, AVCodec** codec = nullptr);

    /// @brief 从AVStream中初始化编/解码器上下文和编/解码器
    /// @param stream AVStream*流
    /// @param is_decoder 是否为解码器 true:解码器 false:编码器
    void InitFromStream(AVStream* stream, bool is_decoder);
    
    /// @brief 释放编解码器上下文
    static void FreeCodecCtx(AVCodecContext* codec_ctx) noexcept;

    /// @brief 找到编解码器
    /// @param codec_id 编/解码器ID
    /// @param is_decoder 是否为解码器 true:解码器 false:编码器
    /// @return 编/解码器，失败则抛异常
    static const AVCodec* FindCodecById(AVCodecID codec_id, bool is_decoder);

    static const AVCodec* FindCodecByName(const std::string& codec_name, bool is_decoder);
    /// @brief 设置编/解码器参数
    /// @param codec 编/解码器
    /// @param width 视频宽度
    /// @param height 视频高度
    /// @param fps 视频帧率
    /// @param pix_fmt 视频像素格式
    /// @param bit_rate 视频码率
    static void SetVideoCodecParameters(AVCodecContext* codec_ctx, int width, int height, double fps, AVPixelFormat pix_fmt, int bit_rate) noexcept;
    
    /// @brief 设置视频质量参数
    /// @param codec_ctx 编/解码器上下文
    /// @param gop_size GOP大小
    /// @param max_b_frames 最大B帧数量
    /// @param flags 编码标志
    static void SetVideoQualityCodecParameters(AVCodecContext* codec_ctx, int gop_size, int max_b_frames, int flags) noexcept;
    
    /// @brief 设置音频质量参数，通过声道数设置默认声道布局
    /// @param codec_ctx 编/解码器上下文
    /// @param sample_rate 采样率
    /// @param channels 声道数
    /// @param sample_fmt 采样格式
    /// @param bit_rate 码率
    static void SetAudioCodecParameters(AVCodecContext* codec_ctx, int sample_rate, int channels, AVSampleFormat sample_fmt, int bit_rate) noexcept;
    
    /// @brief 设置音频质量参数
    /// @param codec_ctx 编/解码器上下文
    /// @param flags 编解码标志
    static void SetAudioQualityCodecParameters(AVCodecContext* codec_ctx, int flags) noexcept;
    /// @brief 获取内部的AVCodecContext指针
    AVCodecContext* get() noexcept;

    /// @brief 获取内部的AVCodecContext指针（常量版本）
    const AVCodecContext* get() const noexcept;

    /// @brief 获取内部的AVCodecContext指针
    AVCodecContext* raw() noexcept;

    /// @brief 获取内部的AVCodecContext指针（常量版本）
    const AVCodecContext* raw() const noexcept;

    /// @brief 打开编/解码器
    void Open(AVDictionary** options = nullptr);

    /// @brief 解复用Demuxer发送压缩后的视频/音频数据到解码器Decoder
    /// @note "没有更多数据包要解码了"，触发解码器输出缓冲的帧
    /// @param pkt 压缩后的视频/音频数据
    /// @return FFmpegResult 成功:FFmpegResult::Success 失败:FFmpegResult::Error
    FFmpegResult SendPacket(const ::AVPacket* pkt) noexcept;
    
    /// @brief 发送空数据到解码器Decoder/Encoder，用于刷新编/解码器缓冲区
    /// @return FFmpegResult 成功:FFmpegResult::ENDFILE 错误:FFmpegResult::Error
    FFmpegResult SendNullPacket() noexcept;

    /// @brief 从解码器接收原始视频/音频数据
    /// @param frame 原始视频/音频数据
    /// @return FFmpegResult 成功:FFmpegResult::Success 失败:FFmpegResult::Error
    FFmpegResult ReceiveFrame(::AVFrame* frame) noexcept;

    /// @brief 向编码器发送原始视频/音频数据
    /// @param frame 原始视频/音频数据
    /// @return FFmpegResult 成功:FFmpegResult::Success 失败:FFmpegResult::Error
    FFmpegResult SendFrame(const AVFrame* frame) noexcept;
    
    /// @brief 从编码器接收压缩后的视频/音频数据
    /// @param pkt 压缩后的视频/音频数据
    /// @return FFmpegResult 成功:FFmpegResult::Success 失败:FFmpegResult::Error
    FFmpegResult ReceivePacket(AVPacket* pkt) noexcept;

    /// @brief 清理并刷新编码器的缓冲区，可能产生额外的输出packet
    FFmpegResult Flush(AVPacket* pkt) noexcept;
#if 0
    /// @brief 设置硬件设备上下文
    /// @param hw_device_ctx 硬件设备上下文
    /// @return true 成功 false 失败
    bool SetHWDevice(AVHWDeviceType type, const std::string& device_name = "");

    /// @brief 设置硬件设备上下文池
    void SetDevicePool(std::shared_ptr<HWDevicePool> pool);

    /// @brief 设置硬件设备上下文句柄
    /// @param handle 硬件设备句柄
    /// @return true 成功 false 失败
    bool SetHWDeviceHandle(const DeviceHandle& handle);

    /// @brief 从GPU内存中复制数据到CPU内存
    /// @param src 源帧
    /// @param dst 目标帧
    /// @return true 成功 false 失败
    bool TransferFrameToCPU(const Frame& src, Frame& dst);
    
    /// @brief 检查是否启用了硬件设备
    /// @return true 启用了硬件设备 false 未启用
    bool isHWActive() const noexcept;

    /// @brief 获取硬件设备上下文
    /// @return AVBufferRef* 硬件设备上下文
    AVBufferRef* getHWDeviceContext() noexcept;

    /// @brief 获取硬件设备上下文（常量版本）
    /// @return AVBufferRef* 硬件设备上下文
    const AVBufferRef* getHWDeviceContext() const noexcept;
#endif
    /// @brief 获取接收到的帧数（解码器）
    std::atomic<uint64_t>& getFrameRecv() noexcept;
    /// @brief 获取发送的包数（解码器）
    std::atomic<uint64_t>& getPacketSend() noexcept;

    /// @brief 获取发送的帧数（编码器）
    std::atomic<uint64_t>& getFrameSend() noexcept;
    /// @brief 获取接收的包数（编码器）
    std::atomic<uint64_t>& getPacketRecv() noexcept;

protected:
    AVCodecContext* codec_ctx_ = nullptr;
    AVCodec* codec_ = nullptr;
private:
    void move_from(CodecContext& other) noexcept;
    void free() noexcept;

    inline FFmpegResult map_ret(int av_ret, bool is_sender) {
        if (av_ret == 0) {
            return FFmpegResult::TRUE;
        }
        if (av_ret == AVERROR(EAGAIN)) {
            return is_sender ? FFmpegResult::SEND_AGAIN : FFmpegResult::RECV_AGAIN;
        }
        if (av_ret == AVERROR_EOF) {
            return FFmpegResult::ENDFILE;
        }
        return FFmpegResult::ERROR;
    }

    bool is_hw_active_ = false;
    // HWDeviceContext hw_device_ctx_; // 硬件设备上下文
    // AVPixelFormat hw_pix_fmt_ = AV_PIX_FMT_NONE;
    // std::shared_ptr<HWDevicePool> hw_device_pool_;  // 硬件设备池

    /// @brief 接收到的帧数（解码器）
    std::atomic<uint64_t> frames_recv_{ 0 };
    /// @brief 发送的包数（解码器）
    std::atomic<uint64_t> packets_send_{ 0 };

    /// @brief 接收到的包数（编码器）
    std::atomic<uint64_t> packets_recv_{ 0 };
    /// @brief 发送的帧数（编码器）
    std::atomic<uint64_t> frames_send_{ 0 };

};

// class CodecSession {
// public:
//     const int DECODE_SLEEP_US = 10000;    // 10ms
//     const int DECODE_MAX_RETRY = 1000;  // 1000次
//     const int ENCODE_SLEEP_US = 10000;  // 10ms
//     const int ENCODE_MAX_RETRY = 1000;  // 1000次
//     using ReadFrameFunc = FFmpegResult(*)(AVFormatContext* fmt_ctx, ::AVPacket* pkt, int time_out);
//     using SendPacketFunc = FFmpegResult(*)(AVCodecContext* codec_ctx, const ::AVPacket* pkt);
//     using SendNullPacketFunc = FFmpegResult(*)(AVCodecContext* codec_ctx);
//     using RecvFrameFunc = FFmpegResult(*)(AVCodecContext* codec_ctx, ::AVFrame* frame);
//     using FlushFunc = FFmpegResult(*)(AVCodecContext* codec_ctx, AVPacket* pkt);

//     FFmpegResult Decode(::AVFrame* out_frame, int time_out = 0,
//         ReadFrameFunc read_fc, SendPacketFunc send_fc, SendNullPacketFunc send_null_fc, RecvFrameFunc recv_fc,
//         AVFormatContext* fmt_ctx, AVCodecContext* codec_ctx,
//         int target_stream_index);

//     using SendFrameFunc = FFmpegResult(*)(AVCodecContext* codec_ctx, const ::AVFrame* frame);
//     using RecvPacketFunc = FFmpegResult(*)(AVCodecContext* codec_ctx, ::AVPacket* pkt);

//     FFmpegResult Encode(::AVFrame* in_frame, AVPacket* out_pkt, int time_out = 0,
//         SendFrameFunc send_fc, RecvPacketFunc recv_fc,
//         AVFormatContext* fmt_ctx, AVCodecContext* codec_ctx);
// };


}

