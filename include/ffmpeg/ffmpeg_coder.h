#include "ffmpeg_codec.h"
#include "ffmpeg_avformat.h"
#include "ffmpeg_log.h"
extern  "C" {
#include <libavcodec/avcodec.h>
}

namespace FFmpeg { 

/// @brief 音频编解码参数
struct AudioCodecParams {
    AudioCodecParams(std::string c, int s, int ch, AVSampleFormat fmt, int b)
        : codec_name(c), sample_rate(s), channels(ch), sample_fmt(fmt), bit_rate(b) 
    {
        av_channel_layout_default(&channel_layout, channels > 0 ? channels : 0);
    }

    ~AudioCodecParams() {
        av_channel_layout_uninit(&channel_layout);
    }

    /// @brief 拷贝构造函数(深拷贝)
    AudioCodecParams(const AudioCodecParams& other) : codec_name(other.codec_name),
        sample_rate(other.sample_rate), channels(other.channels),
        sample_fmt(other.sample_fmt), bit_rate(other.bit_rate) 
    {
        if (av_channel_layout_copy(&channel_layout, &other.channel_layout) < 0) {
            // throw std::runtime_error("av_channel_layout_copy failed");
            MLOG_WARN("av_channel_layout_copy failed");
            av_channel_layout_uninit(&channel_layout);
            channel_layout.order = AV_CHANNEL_ORDER_UNSPEC;
            channel_layout.nb_channels = 0;
        }
    }

    /// @brief 赋值运算符
    AudioCodecParams& operator=(const AudioCodecParams& other) { 
        if (this != &other) {
            // 先清理
            av_channel_layout_uninit(&channel_layout);

            // 拷贝基本成员
            codec_name = other.codec_name;
            sample_rate = other.sample_rate;
            channels = other.channels;
            sample_fmt = other.sample_fmt;
            bit_rate = other.bit_rate;
            // 拷贝声道布局
            if (av_channel_layout_copy(&channel_layout, &other.channel_layout) < 0) {
                // throw std::runtime_error("av_channel_layout_copy failed");
                MLOG_WARN("av_channel_layout_copy failed");
                av_channel_layout_uninit(&channel_layout);
                channel_layout.order = AV_CHANNEL_ORDER_UNSPEC;
                channel_layout.nb_channels = 0;
            }
        }
        return *this;
    }

    /// @brief 移动构造函数
    AudioCodecParams(AudioCodecParams&& other) noexcept 
        : codec_name(std::move(other.codec_name)),
        sample_rate(other.sample_rate),
        channels(other.channels),
        sample_fmt(other.sample_fmt),
        channel_layout(other.channel_layout),
        bit_rate(other.bit_rate)
    {
        // memset(&other.channel_layout, 0, sizeof(other.channel_layout));
        other.channel_layout = AVChannelLayout{};
    }

    /// @brief 移动赋值运算符
    AudioCodecParams& operator=(AudioCodecParams&& other) noexcept {
        if (this != &other) {
            // 先清理
            av_channel_layout_uninit(&channel_layout);

            // 移动基本成员
            codec_name = std::move(other.codec_name);
            sample_rate = other.sample_rate;
            channels = other.channels;
            sample_fmt = other.sample_fmt;
            bit_rate = other.bit_rate;
            // 移动声道布局
            channel_layout = other.channel_layout;

            other.channel_layout = AVChannelLayout{};
        }
        return *this;
    }

    /// @brief 判断声道布局是否有效
    bool hasValidChannelLayout() const {
        return av_channel_layout_check(&channel_layout) == 1;
    }

    /// @brief 判断声道布局是否为标准布局
    bool hasStandardChannelLayout() const {
        return channel_layout.order == AV_CHANNEL_ORDER_NATIVE;
    }
    /// @brief 获取声道布局名称
    std::string getChannelLayoutName() const {
        char buf[256];
        if (av_channel_layout_describe(&channel_layout, buf, sizeof(buf)) > 0) {
            return std::string(buf);
        }
        return "";
    }

    std::string codec_name; // 编码器名称
    int sample_rate = -1;   // 输出采样率（-1表示使用输入采样率）
    int channels = -1;      // 输出声道数（-1表示使用输入声道数）一般输入2(stereo) 1(mono) 6(5.1) 8(7.1),输入不安全值channel_layout会变成UNSPEC
    AVSampleFormat sample_fmt = AV_SAMPLE_FMT_NONE; // 输出样本格式（AV_SAMPLE_FMT_NONE表示使用输入格式）
    AVChannelLayout channel_layout; // 输出声道布局
    int bit_rate = -1;      // 输出比特率（-1表示使用输入比特率）
};

/// @brief 解码器
class VideoDecoder : protected CodecContext, protected FormatContext {

public:
    /// @brief 构造函数 创建解码器
    /// @param url 视频文件路径
    VideoDecoder(const std::string& url, bool is_hw = false, AVDictionary** options = nullptr);

    /// @brief 析构函数
    ~VideoDecoder() = default;

    /// @brief 解码
    FFmpegResult Decode(AVFrame* out_frame, int timeout = 0);

    /// @brief 获取内部的AVCodecContext指针
    AVCodecContext* get() noexcept;
    AVCodecContext* raw() noexcept;

    /// @brief 获取内部的AVCodecContext指针（常量版本）
    const AVCodecContext* get() const noexcept;
    const AVCodecContext* raw() const noexcept;

    //TODO:添加硬件相关接口
    // bool InitHWEncoder(AVHWDeviceType type);

    /// @brief 获取视频的宽度
    int width() const noexcept;
    /// @brief 获取视频的高度
    int height() const noexcept;
    /// @brief 获取视频的像素格式
    AVPixelFormat pix_fmt() const noexcept;
    /// @brief 获取视频的帧率
    double fps() const noexcept;
    /// @brief 获取视频的时长（秒）
    double duration() const noexcept;
    /// @brief 获取视频流的索引
    int stream_idx() const noexcept;
    /// @brief 获取视频流的时间基
    AVRational time_base() const noexcept;
private:
    FFmpeg::Stream stream_;
};


class VideoEncoder : protected CodecContext, protected FormatContext {
public:
    
    /// @brief 构造函数 创建编码器
    /// @param codec_name 编码器名称
    /// @param width 视频宽度
    /// @param height 视频高度
    /// @param fps 视频帧率
    /// @param pix_fmt 视频像素格式
    /// @param bit_rate 视频码率
    VideoEncoder(const std::string& codec_name, int width, int height, double fps, AVPixelFormat pix_fmt, int bit_rate, bool is_hw = false, AVDictionary** options = nullptr);
    
    /// @brief 构造函数 创建编码器
    /// @param params 编码视频参数
    VideoEncoder(const VideoCodecParams& params, bool is_hw = false, AVDictionary** options = nullptr);

    ~VideoEncoder() = default;

    /// @brief 获取内部的AVCodecContext指针
    AVCodecContext* get() noexcept;
    AVCodecContext* raw() noexcept;

    /// @brief 获取内部的AVCodecContext指针（常量版本）
    const AVCodecContext* get() const noexcept;
    const AVCodecContext* raw() const noexcept;

    FFmpegResult Encode(AVFrame* in_frame, AVPacket* out_pkt, int time_out = 0);

    /// @brief 设置默认的编/解码参数
    /// @param is_decoder 是否为解码器 true:解码器 false:编码器
    /// @note  解码时从 AVStream 中获取参数，编码时手动设置默认编码参数
    // void SetDefaultCodecParameters(AVStream* stream, bool is_decoder = true);
    
    // bool InitHWEncoder(AVHWDeviceType type);
    FFmpegResult Flush(AVPacket* out_pkt);

    /// @brief 获取视频的宽度
    int width() const noexcept;
    /// @brief 获取视频的高度
    int height() const noexcept;
    /// @brief 获取视频的像素格式
    AVPixelFormat pix_fmt() const noexcept;
    /// @brief 获取视频的帧率
    double fps() const noexcept;
    /// @brief  获取视频的码率
    int bit_rate() const noexcept;
    /// @brief 获取视频流的时间基
    AVRational time_base() const noexcept;

private:
    int width_;
    int height_;
    double fps_;
    AVPixelFormat pix_fmt_;
    int bit_rate_;
    int stream_idx_;
    AVRational time_base_;
};


class AudioDecoder :protected CodecContext, protected FormatContext {
public:
    /// @brief 构造函数 创建解码器
    /// @param url 视频文件路径
    AudioDecoder(const std::string& url, bool is_hw = false, AVDictionary** options = nullptr);

    ~AudioDecoder() = default;

    /// @brief 解码
    FFmpegResult Decode(AVFrame* out_frame, int timeout = 0);

    /// @brief 获取内部的AVCodecContext指针
    AVCodecContext* get() noexcept;
    AVCodecContext* raw() noexcept;

    /// @brief 获取内部的AVCodecContext指针（常量版本）
    const AVCodecContext* get() const noexcept;
    const AVCodecContext* raw() const noexcept;

    //TODO:添加硬件相关接口
    // bool InitHWEncoder(AVHWDeviceType type);

    /// @brief 获取音频的采样率
    int sample_rate() const noexcept;
    /// @brief 获取音频的声道数
    int channels() const noexcept;
    /// @brief 获取音频的样本格式
    AVSampleFormat sample_fmt() const noexcept;
    /// @brief 获取音频的比特率
    int bit_rate() const noexcept;
    /// @brief 获取音频的通道布局
    AVChannelLayout channel_layout() const noexcept;
    /// @brief 获取音频的时长
    double duration() const noexcept;
    /// @brief 获取音频的流索引
    int stream_idx() const noexcept;
    /// @brief 获取视频流的时间基
    AVRational time_base() const noexcept;

private:
    FFmpeg::Stream stream_;
};

class AudioEncoder :protected CodecContext, protected FormatContext {
public:
    
    /// @brief 构造函数 创建音频编码器
    /// @param codec_name 编码器名字
    /// @param sample_rate 采样率
    /// @param channels 声道数
    /// @param sample_fmt 采样格式
    /// @param bit_rate 码率
    /// @param is_hw 是否使用硬件编码
    /// @param options 编码参数
    AudioEncoder(const std::string& codec_name, int sample_rate, int channels, 
        AVSampleFormat sample_fmt, int bit_rate, bool is_hw = false, AVDictionary** options = nullptr);
    
    AudioEncoder(const AudioCodecParams& params, bool is_hw = false, AVDictionary** options = nullptr);
    ~AudioEncoder() = default;
    
    AVCodecContext* get() noexcept;
    AVCodecContext* raw() noexcept;
    const AVCodecContext* get() const noexcept;
    const AVCodecContext* raw() const noexcept;

    /// @brief 编码
    FFmpegResult Encode(AVFrame* frame, AVPacket* out_pkt, int timeout = 0);

    FFmpegResult Flush(AVPacket* out_pkt);

    /// @brief 获取音频的采样率
    int sample_rate() const noexcept;
    int channels() const noexcept;
    AVSampleFormat sample_fmt() const noexcept;
    int bit_rate() const noexcept;
    AVChannelLayout channel_layout() const noexcept;
    AVRational time_base() const noexcept;

private:
    int sample_rate_;
    int channels_;
    AVSampleFormat sample_fmt_;
    int bit_rate_;
    AVChannelLayout channel_layout_;
    int stream_idx_;
    AVRational time_base_;
};

}


