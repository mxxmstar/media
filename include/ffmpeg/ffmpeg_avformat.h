#pragma once

extern "C" {
#include "libavformat/avformat.h"
#include "libavutil/error.h"
#include "libavcodec/avcodec.h"
}

#include <string>
#include <stdexcept>
#include <memory>

#include "ffmpeg_avutil.h"

namespace FFmpeg {

class Packet;
/// @brief 封装AVFormatContext
class FormatContext {
public:

    /// @brief 默认构造函数
    /// @note 内部ctx_为nullptr，需要调用Init函数进行初始化
    /// @note 定义默认构造函数，让子类不需要在初始化列表中进行初始化
    FormatContext() = default;
    /// @brief 禁止拷贝
    FormatContext(const FormatContext &) = delete;

    /// @brief 禁止赋值
    FormatContext &operator=(const FormatContext &) = delete;

    /// @brief 移动构造
    FormatContext(FormatContext && other) noexcept;

    /// @brief 移动赋值
    FormatContext &operator=(FormatContext && other) noexcept;

    /// @brief 构造函数，打开文件/流
    /// @param url 输入媒体的url
    /// @param fmt 输入格式，nullptr表示自动识别
    /// @param options 选项，nullptr表示无选项
    explicit FormatContext(const std::string& url, AVInputFormat* fmt = nullptr, AVDictionary** options = nullptr);
    
    /// @brief 构造函数 输出文件/流
    /// @param url 输出媒体的url
    /// @param fmt 输出格式，nullptr表示自动识别
    /// @param options 输出选项，nullptr表示无选项
    explicit FormatContext(const std::string& url, AVOutputFormat* fmt = nullptr, AVDictionary** options = nullptr);

    /// @brief 初始化函数，打开文件/流，失败抛异常
    /// @param url 媒体url
    /// @param fmt 输入格式，nullptr表示自动识别
    /// @param options 选项，nullptr表示无选项
    void InitInCtx(const std::string& url, AVInputFormat* fmt = nullptr, AVDictionary** options = nullptr);

    /// @brief 初始化函数，输出文件/流，失败抛异常
    /// @param url 媒体url
    /// @param fmt 输出格式，nullptr表示自动识别
    /// @param options 选项，nullptr表示无选项
    void InitOutCtx(const std::string& url, AVOutputFormat* fmt = nullptr, AVDictionary** options = nullptr);

    /// @brief 手动清理输入/输出文件/流
    void Cleanup() noexcept;

    /// @brief 打开输出文件并写入头信息
    /// @param url 输出媒体的url
    /// @param fmt 输出格式，nullptr表示自动识别
    /// @param options 输出选项，nullptr表示无选项
    /// @return 成功返回FFmpegResult::TRUE，失败抛异常
    FFmpegResult OpenAndWriteHeader(const std::string& url, const AVOutputFormat* fmt = nullptr, AVDictionary** options = nullptr);

    /// @brief 析构函数，关闭文件/流
    ~FormatContext();

    /// @brief 获取内部指针
    ::AVFormatContext* get() noexcept;

    /// @brief 获取内部指针
    const ::AVFormatContext* get() const noexcept;
    
    /// @brief 获取内部指针
    ::AVFormatContext* raw() noexcept;

    /// @brief 获取内部指针
    const ::AVFormatContext* raw() const noexcept;

    /// @brief 获取流类型
    /// @param stream_index 流索引
    /// @return 流类型
    AVMediaType stream_type(int stream_index) const;
    
    /// @brief 读取一帧数据到AVPacket中
    /// @param pkt 数据包
    /// @return 成功返回FFmpegResult::FFMPEG_TRUE，到达文件末尾返回FFmpegResult::FFMPEG_FALSE，失败、错误返回FFmpegResult::FFMPEG_ERROR
    FFmpegResult ReadFrame(::AVPacket* pkt); 

    /// @brief 读取一帧数据到AVPacket中(超时版)
    /// @param pkt 数据包
    /// @param time_out 超时时间 单位:毫秒
    /// @return 成功返回FFmpegResult::FFMPEG_TRUE，到达文件末尾返回FFmpegResult::FFMPEG_FALSE，失败、错误返回FFmpegResult::FFMPEG_ERROR
    FFmpegResult ReadFrame(::AVPacket* pkt, int time_out = 0);
    

    /// @brief 写入一帧数据到文件/流中
    /// @param pkt 数据包
    /// @param time_out 超时时间 单位:毫秒
    /// @return 成功返回FFmpegResult::FFMPEG_TRUE，失败、错误返回FFmpegResult::FFMPEG_ERROR
    FFmpegResult WritePacket(::AVPacket* pkt, int time_out = 0);

    /// @brief 写入一帧数据到文件/流中(超时版)
    /// @param pkt 数据包
    /// @param time_out 超时时间 单位:毫秒
    /// @return 成功返回FFmpegResult::FFMPEG_TRUE，失败、错误返回FFmpegResult::FFMPEG_ERROR
    FFmpegResult WritePacket(::AVPacket* pkt);

    /// @brief 帧跳转
    /// @param timestamp 时间戳
    /// @param stream_index 流索引
    /// @param flag 标志
    /// @return 成功返回FFmpegResult::FFMPEG_TRUE，失败返回FFmpegResult::FFMPEG_ERROR
    FFmpegResult Seek(int64_t timestamp, int64_t stream_index, int flag = AVSEEK_FLAG_BACKWARD);

    /// @brief 精确帧跳转
    /// @param timestamp 时间戳
    /// @param stream_index 流索引 -1:所有流
    /// @param range_sec 时间范围 单位:秒
    /// @return 成功返回FFmpegResult::FFMPEG_TRUE，失败返回FFmpegResult::FFMPEG_ERROR
    FFmpegResult SeekPrecise(int64_t timestamp, int64_t stream_index, int64_t range_sec = 1);
    
    /// @brief dump信息
    /// @param url url
    /// @param isOutput 0:输入 1:输出
    void dump(const std::string& url, int isOutput = 0) const;

    /// @brief 工厂函数，创建解复用器上下文
    /// @param url 输入视频的url
    /// @param fmt 输入格式，nullptr表示自动识别
    /// @param options 选项，nullptr表示无选项
    /// @return 解复用器上下文
    static AVFormatContext* CreateInFmtCtx(const std::string& url, AVInputFormat* fmt = nullptr, AVDictionary** options = nullptr);

    /// @brief 工厂函数，创建复用器上下文
    /// @param url 输出视频的url
    /// @param fmt 输出格式，nullptr表示自动识别
    /// @param options 选项，nullptr表示无选项
    /// @return 创建的复用器上下文
    static AVFormatContext* CreateOutFmtCtx(const std::string& url, AVOutputFormat* fmt = nullptr, AVDictionary** options = nullptr);
    
    /// @brief 工厂函数，先alloc，再手动配置上下文参数
    // static AVFormatContext* Alloc();

    /// @brief 工厂函数，释放解复用器上下文
    static void CleanupInFmtCtx(AVFormatContext* ctx) noexcept;

    /// @brief 工厂函数，释放复用器上下文
    static void CleanupOutFmtCtx(AVFormatContext* ctx) noexcept;
    
    /// @brief 工厂函数，读取一帧数据到AVPacket中
    static FFmpegResult ReadFrame(AVFormatContext* ctx, ::AVPacket* pkt);
    
    /// @brief 私有构造函数，用于智能指针管理//TODO
    explicit FormatContext(::AVFormatContext* ctx) noexcept;
protected:
    ::AVFormatContext* fmt_ctx_ = nullptr;
private:

    /// @brief 当前上下文是否为输出（复用器）
    /// 当通过 InitOutCtx/输出构造函数创建时为 true；
    /// 通过 InitInCtx/输入构造函数创建时为 false；
    /// 通过显式 AVFormatContext* 构造函数时依据 ctx->oformat 是否非空自动判断。
    bool is_output_ = false;

    void move_from(FormatContext& other) noexcept;

};

/// @brief 封装AVStream，拥有指针
class Stream {
public:
    /// @brief 构造函数，内部stream_为nullptr
    Stream() = default;

    /// @brief 构造函数，stream_会被赋值为stream，接管stream的生命周期
    explicit Stream(AVStream* stream);

    /// @brief 析构函数
    ~Stream() noexcept;

    // 禁止拷贝，允许移动
    Stream(const Stream&) = delete;
    
    /// @brief 禁止赋值
    Stream& operator=(const Stream&) = delete;
    
    /// @brief 移动构造函数
    Stream(Stream&& other) noexcept;
    
    /// @brief 移动赋值函数
    Stream& operator=(Stream&& other) noexcept;

    /// @brief 获取内部AVStream*指针
    AVStream* raw() noexcept;

    /// @brief 获取内部AVStream*指针
    const AVStream* raw() const noexcept;

    /// @brief 流索引
    int index() const noexcept;

    /// @brief 流类型
    AVMediaType type() const noexcept;
    
    /// @brief 流编码ID
    AVCodecID codec_id() const noexcept;

    /// @brief 时间基
    AVRational time_base() const noexcept;

    /// @brief 持续时间
    int64_t duration() const noexcept;

    /// @brief 总帧数
    int64_t nb_frames() const noexcept;

    void copy_from(AVCodecContext* codecCtx);

    /// @brief 复制流参数到CodecContext
    /// @param codecCtx 目标CodecContext
    void copy_to(AVCodecContext* codecCtx) const;

    /// @brief 获取元数据
    /// @param key 元数据键
    /// @return 元数据值
    std::string meta_data(const std::string& key) const;

    /// @brief 工厂函数，创建AVStream流
    static AVStream* CreateStream(AVFormatContext* fmt_ctx);

private:
    /// @brief 非拥有指针
    AVStream* stream_ = nullptr;
};
    
}

