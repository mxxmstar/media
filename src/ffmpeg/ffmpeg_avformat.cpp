#include "ffmpeg_avformat.h"
#include "ffmpeg_codec.h"
#include "ffmpeg_avutil.h"
#include <chrono>
#include <iostream>
extern "C" {
#include "libavutil/time.h"
}
namespace FFmpeg {
const int READ_SLEEP_TIME = 10 * 1000;
FormatContext::FormatContext(FormatContext && other) noexcept {
    move_from(other);
}

FormatContext& FormatContext::operator=(FormatContext && other) noexcept {
    if (this != &other) {
        Cleanup();
        move_from(other);
    }
    return *this;
}

FormatContext::FormatContext(const std::string& url, AVInputFormat* fmt, AVDictionary** options) 
    : is_output_(false) {
    
    if(avformat_open_input(&fmt_ctx_ , url.c_str(), fmt, options) < 0) {
        throw std::runtime_error("avformat_open_input failed, url is:" + url);
    }
    if(avformat_find_stream_info(fmt_ctx_ , nullptr) < 0) {
        Cleanup();
        throw std::runtime_error("avformat_find_stream_info failed");
    }        
}

FormatContext::FormatContext(const std::string& url, AVOutputFormat* fmt, AVDictionary** options) 
    : is_output_(true) {
    if(avformat_alloc_output_context2(&fmt_ctx_ , fmt, nullptr, url.c_str()) < 0) {
        throw std::runtime_error("avformat_alloc_output_context2 failed, url is:" + url);
    }
}

void FormatContext::Cleanup() noexcept {
    if (fmt_ctx_) {
        if (is_output_) {
            if (fmt_ctx_ ->pb) {
                int ret = av_write_trailer(fmt_ctx_);
                if(ret < 0) {
                    char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
                    av_strerror(ret, errbuf, sizeof(errbuf));
                    std::cout << "av_write_trailer failed: " << errbuf << std::endl;
                    // throw std::runtime_error("av_write_trailer failed");
                }
            }
            avformat_free_context(fmt_ctx_);
        } else {
            avformat_close_input(&fmt_ctx_);   
        }
        fmt_ctx_ = nullptr;
    }
}

void FormatContext::InitInCtx(const std::string& url, AVInputFormat* fmt, AVDictionary** options) {
    fmt_ctx_  = CreateInFmtCtx(url, fmt, options);
    is_output_ = false;
}

void FormatContext::InitOutCtx(const std::string& url, AVOutputFormat* fmt, AVDictionary** options) {
    fmt_ctx_  = CreateOutFmtCtx(url, fmt, options);
    is_output_ = true;
}

FFmpegResult FormatContext::OpenAndWriteHeader(const std::string& url, const AVOutputFormat* fmt, AVDictionary** options) {
    // 打开输出文件流
    if (avio_open(&fmt_ctx_ ->pb, url.c_str(), AVIO_FLAG_WRITE) < 0) {
        Cleanup();
        throw std::runtime_error("avio_open failed, url is:" + url);
    }

    // 写入文件头信息
    if(avformat_write_header(fmt_ctx_ , options) < 0) {
        Cleanup();
        throw std::runtime_error("avformat_write_header failed");
    }
    return FFmpegResult::TRUE;
}

FormatContext::~FormatContext() {
    Cleanup();
}

::AVFormatContext* FormatContext::get() noexcept {
    return fmt_ctx_ ;
}

const ::AVFormatContext* FormatContext::get() const noexcept {
    return fmt_ctx_ ;
}

::AVFormatContext* FormatContext::raw() noexcept {
    return fmt_ctx_ ;
}

const AVFormatContext* FormatContext::raw() const noexcept {
    return fmt_ctx_ ;
}

AVMediaType FormatContext::stream_type(int stream_index) const {
    if (stream_index < 0 || stream_index >= fmt_ctx_ ->nb_streams)
        return AVMEDIA_TYPE_UNKNOWN;
    return fmt_ctx_ ->streams[stream_index]->codecpar->codec_type;
}

AVFormatContext* FormatContext::CreateInFmtCtx(const std::string& url, AVInputFormat* fmt, AVDictionary** options) {
    AVFormatContext* ctx = nullptr;
    std::cout << "open input url:" << url.c_str() << std::endl;
    int ret = avformat_open_input(&ctx, url.c_str(), fmt, options);
    if(ret < 0) {
        std::cout << "avformat_open_input failed, code is:" << FFmpeg::tools::av_err(ret) << std::endl;
        throw std::runtime_error("avformat_open_input failed, url is:" + url);
    }
    if(avformat_find_stream_info(ctx, nullptr) < 0) {
        CleanupInFmtCtx(ctx);
        throw std::runtime_error("avformat_find_stream_info failed");
    }
    return ctx;    
}
AVFormatContext* FormatContext::CreateOutFmtCtx(const std::string& url, AVOutputFormat* fmt, AVDictionary** options) { 
    AVFormatContext* ctx = nullptr;
    if (avformat_alloc_output_context2(&ctx, fmt, nullptr, url.c_str()) < 0) {
        CleanupOutFmtCtx(ctx);
        throw std::runtime_error("avformat_alloc_output_context2 failed, url is:" + url);
    }
    return ctx;
}

void FormatContext::CleanupInFmtCtx(AVFormatContext* ctx) noexcept {
    if(ctx) {
        avformat_close_input(&ctx);
    }
}

void FormatContext::CleanupOutFmtCtx(AVFormatContext* ctx) noexcept {
    if (ctx) {
        av_write_trailer(ctx);
        avformat_free_context(ctx);// 内部已处理 AVFMT_NOFILE
    }
}

FFmpegResult FormatContext::ReadFrame(AVPacket* pkt) {
    int ret = av_read_frame(fmt_ctx_ , pkt);
    if(ret == 0) {
        return FFmpegResult::TRUE;
    } else if(ret == AVERROR_EOF) {
        return FFmpegResult::ENDFILE;
    } else {
        return FFmpegResult::ERROR;
    }
}

FFmpegResult FormatContext::ReadFrame(::AVPacket* pkt, int time_out) {
    auto start = std::chrono::steady_clock::now();

    while (true) {
        int ret = av_read_frame(fmt_ctx_ , pkt);
        if(ret == 0) {
            return FFmpegResult::TRUE;
        } else if (ret == AVERROR(EAGAIN)) {
            if (time_out > 0) {
                auto now = std::chrono::steady_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
                if(duration.count() >= time_out) {
                    return FFmpegResult::TIMEOUT;
                }
            }
            av_usleep(READ_SLEEP_TIME); // sleep 10ms        
            continue;
        } else if(ret == AVERROR_EOF) {
            return FFmpegResult::ENDFILE;
        } else {
            return FFmpegResult::ERROR;
        }
    }
}

FFmpegResult FormatContext::WritePacket(::AVPacket* pkt, int time_out) { 
    auto start = std::chrono::steady_clock::now();
    while (true) {
        int ret = av_interleaved_write_frame(fmt_ctx_ , pkt);
        if(ret == 0) {
            return FFmpegResult::TRUE;
        } else if (ret == AVERROR(EAGAIN)) {
            if (time_out > 0) {
                auto now = std::chrono::steady_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
                if(duration.count() >= time_out) {
                    return FFmpegResult::TIMEOUT;
                }
            }
            av_usleep(READ_SLEEP_TIME); // sleep 10ms        
            continue;
        } else if(ret == AVERROR_EOF) {
            return FFmpegResult::ENDFILE;
        } else {
            return FFmpegResult::ERROR;
        }
    }
}

FFmpegResult FormatContext::WritePacket(::AVPacket* pkt) {
    int ret = av_interleaved_write_frame(fmt_ctx_, pkt);
    if(ret == 0) {
        return FFmpegResult::TRUE;
    } else if(ret == AVERROR_EOF) {
        return FFmpegResult::ENDFILE;
    } else {
        return FFmpegResult::ERROR;
    }
}

FFmpegResult FormatContext::Seek(int64_t timestamp, int64_t stream_index, int flag) {
    int ret = av_seek_frame(fmt_ctx_ , stream_index, timestamp, flag);
    if(ret < 0) {
        return FFmpegResult::ERROR;
    }
    return FFmpegResult::TRUE;
}

FFmpegResult FormatContext::SeekPrecise(int64_t timestamp, int64_t stream_index, int64_t range_sec) {
    int64_t min_ts = timestamp - range_sec * AV_TIME_BASE;
    int64_t max_ts = timestamp + range_sec * AV_TIME_BASE;
    int ret = avformat_seek_file(fmt_ctx_ , stream_index, min_ts, timestamp, max_ts, 0);
    if(ret < 0) {
        return FFmpegResult::ERROR;
    }
    return FFmpegResult::TRUE;
}

void FormatContext::dump(const std::string& url, int isOutput) const{
    av_dump_format(fmt_ctx_ , 0, url.c_str(), isOutput);
}

FormatContext::FormatContext(::AVFormatContext* ctx) noexcept : fmt_ctx_ (ctx) {
    // 根据 oformat 是否存在判断上下文类型
    is_output_ = (ctx && ctx->oformat != nullptr);
}

void FormatContext::move_from(FormatContext& other) noexcept {
    fmt_ctx_  = other.fmt_ctx_ ;
    is_output_ = other.is_output_;
    other.fmt_ctx_  = nullptr;
}

/***************************** Stream ***************************** */
Stream::Stream(AVStream* stream) : stream_(stream) {
    if (!stream_) {
        throw std::runtime_error("AVStream is null!");
    }
}

Stream::~Stream() noexcept {
}

Stream::Stream(Stream&& other) noexcept : stream_(other.stream_) { 
    other.stream_ = nullptr; 
}


Stream& Stream::operator=(Stream&& other) noexcept {
    if (this != &other) {
        stream_ = other.stream_;
        other.stream_ = nullptr;
    }
    return *this;
}

AVStream* Stream::raw() noexcept {
    return stream_;
}
const AVStream* Stream::raw() const noexcept {
    return stream_;
}

int Stream::index() const noexcept {
    return stream_->index;
}

AVMediaType Stream::type() const noexcept {
    return stream_->codecpar->codec_type;
}
    
AVCodecID Stream::codec_id() const noexcept {
    return stream_->codecpar->codec_id;
}

AVRational Stream::time_base() const noexcept {
    return stream_->time_base;
}

int64_t Stream::duration() const noexcept {
    return stream_->duration;
}

int64_t Stream::nb_frames() const noexcept {
    return stream_->nb_frames;
}

void Stream::copy_from(AVCodecContext* codecCtx) {
    if(avcodec_parameters_from_context(stream_->codecpar, codecCtx) < 0) {
        throw std::runtime_error("avcodec_parameters_from_context failed");
    }
}

void Stream::copy_to(AVCodecContext* codecCtx) const {
    if(avcodec_parameters_to_context(codecCtx, stream_->codecpar) < 0) {
        throw std::runtime_error("avcodec_parameters_to_context failed");
    }
}

std::string Stream::meta_data(const std::string& key) const {
    AVDictionaryEntry* entry = av_dict_get(stream_->metadata, key.c_str(), nullptr, 0);
    return entry ? std::string(entry->value) : std::string{};
}

AVStream* Stream::CreateStream(AVFormatContext* fmt_ctx) {
    if (!fmt_ctx) {
        throw std::runtime_error("AVFormatContext is null!");
    }
    AVStream* stream = avformat_new_stream(fmt_ctx, nullptr);
    if (!stream) {
        throw std::runtime_error("avformat_new_stream failed");
    }
    return stream;
}


}
