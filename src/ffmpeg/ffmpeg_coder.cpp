#include "ffmpeg_coder.h"
extern "C" {
//#include "libavdevice/avdevice.h"
//#include "libavutil/time.h"
// #include <libavdevice/avdevice.h>
#include <libavutil/time.h>
}
namespace FFmpeg {

    const int DECODE_SLEEP_US = 10000;    // 10ms
    const int DECODE_MAX_RETRY = 1000;  // 1000次
    const int ENCODE_SLEEP_US = 10000;  // 10ms
    const int ENCODE_MAX_RETRY = 1000;  // 1000次

/************************************VideoDecodr***********************************/
 VideoDecoder::VideoDecoder(const std::string& url, bool is_hw, AVDictionary** options) {
    // 1. 打开文件，获取文件格式上下文,查找流信息
    FormatContext::InitInCtx(url, nullptr, options);

    // 2. 找到第一个视频流
    AVStream* stream = nullptr;
    int video_index = -1;
    for(unsigned i = 0; i < fmt_ctx_->nb_streams; i++) {
        if (stream_type(i) == AVMEDIA_TYPE_VIDEO) {
            video_index = i;
            stream = fmt_ctx_->streams[i];
            break;
        }
    }
    if(video_index < 0 || !stream) {
        throw std::runtime_error("no video stream found");
    }

    stream_ = Stream(stream);
    
    // 3. 初始化解码器上下文 codec_ codec_ctx_
    // 用stream初始化解码器上下文中已经复制了视频参数
    InitFromStream(stream_.raw(), true);
    
    Open(options);
}

FFmpegResult VideoDecoder::Decode(AVFrame* out_frame, int time_out) { 
    Packet pkt;
    auto start = std::chrono::steady_clock::now();
    int retry_count = 0;
   
    while(true) {
        int remain_ms = 0;
        if (time_out > 0) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed_ms = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count());
            remain_ms = std::max(0, time_out - elapsed_ms);
            if (remain_ms <= 0) {
                return FFmpegResult::TIMEOUT;    // 超时
            }
        }
       
        // 将读取到的数据包发送给解复用器
        FFmpegResult ret = ReadFrame(pkt.raw(), remain_ms);

        // 成功拿到一个packet
        if(ret == FFmpegResult::TRUE) {
            if(pkt.get()->stream_index != stream_.index()) {
               // 拿到的packet不是当前stream的，则跳过继续读取下一个packet
               pkt.unref();
               continue;
            }

            // 解复用将packet送入解码器
            while (true) {
                FFmpegResult sret = this->SendPacket(pkt.raw());
                if (sret == FFmpegResult::TRUE) {
                    // 发送成功
                    pkt.unref();
                   
                    // 尝试从解码器接收帧
                    while (true) {
                        FFmpegResult rret = this->ReceiveFrame(out_frame);
                        if (FFmpegResult::SEND_AGAIN == rret) {
                            break;   // 跳出内层 receive_frame 循环， 重试 send_packet
                        } else {
                            // EOF ERROR TRUE
                            return rret;
                        }   
                    }
                    // 因为send_packet成功，这个pkt被解码器消耗掉了，所以这里需要跳出外层 send_packet 循环 continue 继续读取下一个Frame
                    break;   // 跳出外层 send_packet 循环 continue 继续读取下一个Frame
                } else if (sret == FFmpegResult::RECV_AGAIN) {
                    retry_count = 0;
                    // 解码器的输出缓冲已满，需要先从中取出Frame，解复用器不能再发packet了
                    while(true) {

                        // 尝试从解码器接收帧，腾出空间再send packet
                        FFmpegResult rret = this->ReceiveFrame(out_frame);
                        if (rret == FFmpegResult::TRUE) {
                            // 成功取出一帧
                            pkt.unref();
                            return rret; 
                        } else if (rret == FFmpegResult::SEND_AGAIN) {
                            // 解码流程的逻辑处理出现了偏差，短暂等待后重试
                            // TODO:添加日志
                            // 检查超时
                            if (time_out > 0) {
                                auto now = std::chrono::steady_clock::now();
                                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
                                if (elapsed.count() >= time_out) {
                                    // pkt 自动释放
                                    return FFmpegResult::TIMEOUT;
                                }
                            }

                            if (++retry_count > DECODE_MAX_RETRY) {
                                pkt.unref();
                                return FFmpegResult::ERROR; // 认为是死循环状态
                            }
                            av_usleep(DECODE_SLEEP_US);
                            break;   // 退出内层 receive_frame 循环， 重试 send_packet
                        } else {
                            // 错误 到达末尾
                            pkt.unref();
                            return rret; // ERROR EOF
                        }
                    }
                    continue;    // 继续外层 send_packet
                } else if (sret == FFmpegResult::ENDFILE) {
                    // codec 已经结束, 不能再send packet
                    pkt.unref();
                    return FFmpegResult::ENDFILE;
                } else {
                    // 其他错误
                    pkt.unref();
                    return FFmpegResult::ERROR;
                }
            }   // send packet loop
            continue;    // 继续外层 read Frame loop
        } else if (ret == FFmpegResult::ENDFILE) {
            // demuxer EOF, 向解码器发送空包
            pkt.unref();
            FFmpegResult sret = SendNullPacket();
            if (sret == FFmpegResult::ERROR) {
                return FFmpegResult::ERROR;
            }

            // 从解码器中取出剩余帧
            while (true) { 
                FFmpegResult rret = ReceiveFrame(out_frame);
                if (rret == FFmpegResult::ENDFILE || rret == FFmpegResult::SEND_AGAIN) {
                    return FFmpegResult::ENDFILE;
                } else {
                    // ERR TRUE
                    return rret;
                }
            }
        } else if(ret == FFmpegResult::TIMEOUT){
            return FFmpegResult::TIMEOUT;
        } else {
            return FFmpegResult::ERROR;
        }
    }   // end outer read frame while
   
    // 永远不会到达这里
    return FFmpegResult::ERROR;
}

int VideoDecoder::width() const noexcept {
    return codec_ctx_ ? codec_ctx_->width : 0;
}

int VideoDecoder::height() const noexcept {
    return codec_ctx_ ? codec_ctx_->height : 0;
}

AVPixelFormat VideoDecoder::pix_fmt() const noexcept {
    return codec_ctx_ ? codec_ctx_->pix_fmt : AV_PIX_FMT_NONE;
}

double VideoDecoder::fps() const noexcept {
    if (!stream_.raw()) {
        return 0.0;
    }
    AVRational r = stream_.raw()->avg_frame_rate;
    if (r.den == 0 || r.num == 0) {
        return 0.0;
    }
    return static_cast<double>(r.num / r.den);
}

double VideoDecoder::duration() const noexcept {
    if (!fmt_ctx_ || !stream_.raw()) {
        return 0.0;
    }
    if (stream_.raw()->duration == AV_NOPTS_VALUE) {
        return static_cast<double>(fmt_ctx_->duration) / AV_TIME_BASE;    
    }

    return static_cast<double>(stream_.raw()->duration) * av_q2d(stream_.time_base());
    
}

int VideoDecoder::stream_idx() const noexcept {
    return stream_.index();
}

AVRational VideoDecoder::time_base() const noexcept {
    if (!stream_.raw()) {
        return {0, 1};
    }
    return stream_.raw()->time_base;
}



/************************************VideoEncoder***********************************/
VideoEncoder::VideoEncoder(const std::string& codec_name, int width, int height, double fps, AVPixelFormat pix_fmt, int bit_rate, bool is_hw, AVDictionary** options)
    : CodecContext(codec_name, false), width_(width), height_(height), fps_(fps), pix_fmt_(pix_fmt), bit_rate_(bit_rate) {
    // 配置编码器参数
    SetVideoCodecParameters(codec_ctx_, width_, height_, fps_, pix_fmt_, bit_rate_);
    
    if (is_hw) {
        // SetHWDevice(AV_HWDEVICE_TYPE_CUDA);
    }

    // 打开编码器
    Open(options);
}

VideoEncoder::VideoEncoder(const VideoCodecParams& params, bool is_hw, AVDictionary** options) 
    : CodecContext(params.codec_name, false), width_(params.width), height_(params.height), 
        fps_(params.fps), pix_fmt_(params.pix_fmt), bit_rate_(params.bit_rate) {
    // 配置编码器参数
    SetVideoCodecParameters(codec_ctx_, width_, height_, fps_, pix_fmt_, bit_rate_);
    
    // if (is_hw) {
    //     // SetHWDevice(AV_HWDEVICE_TYPE_CUDA);
    // }
    // 打开编码器
    Open(options);
}

FFmpegResult VideoEncoder::Encode(AVFrame* in_frame, AVPacket* out_pkt, int time_out){
    auto start = std::chrono::steady_clock::now();
    int retry_count = 0;

    if (in_frame == nullptr) {
        FFmpegResult flush_ret = SendFrame(nullptr);
        if (flush_ret == FFmpegResult::ERROR) {
            return FFmpegResult::ERROR;
        }
        return ReceivePacket(out_pkt);
    }
    while (true) {
        FFmpegResult sret = SendFrame(in_frame);
        if (sret == FFmpegResult::TRUE) {
            // send 成功后，清空输入
            av_frame_unref(in_frame);

            FFmpegResult rret = ReceivePacket(out_pkt);
            return rret; 
        } else if (sret == FFmpegResult::RECV_AGAIN) {
            // 队列满，先取出 packet
            FFmpegResult rret = ReceivePacket(out_pkt);
            if (rret == FFmpegResult::SEND_AGAIN) {
                if (++retry_count > ENCODE_MAX_RETRY){
                    return FFmpegResult::ERROR;
                }

                if (time_out > 0) {
                    auto now = std::chrono::steady_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
                    if (elapsed.count() >= time_out) {
                        // pkt 自动释放
                        return FFmpegResult::TIMEOUT;
                    }
                }
                av_usleep(ENCODE_SLEEP_US);
                continue;
            } else {
                return rret;
            }
        } else {
            // EOF ERROR
            return sret;
        }
    }
   return FFmpegResult::ERROR;
}

// 新增：刷新编码器，取出剩余的编码包
FFmpegResult VideoEncoder::Flush(AVPacket* out_pkt) {
    return CodecContext::Flush(out_pkt);
}

AVCodecContext* VideoEncoder::raw() noexcept {
    return codec_ctx_;
}

const AVCodecContext* VideoEncoder::raw() const noexcept {
    return codec_ctx_;
}

AVCodecContext* VideoEncoder::get() noexcept {
    return codec_ctx_;
}

const AVCodecContext* VideoEncoder::get() const noexcept {
    return codec_ctx_;
}

int VideoEncoder::width() const noexcept {
    return width_;
}

int VideoEncoder::height() const noexcept {
    return height_;
}

double VideoEncoder::fps() const noexcept {
    return fps_;
}

AVPixelFormat VideoEncoder::pix_fmt() const noexcept {
    return pix_fmt_;
}

int VideoEncoder::bit_rate() const noexcept {
    return bit_rate_;
}

AVRational VideoEncoder::time_base() const noexcept {
    return codec_ctx_ ? codec_ctx_->time_base : av_make_q(1, 1000);
}

/***********************************AudioDecoder*********************************/
AudioDecoder::AudioDecoder(const std::string& url, bool is_hw, AVDictionary** options) {
    // 1. 打开文件，获取文件格式上下文,查找流信息
    FormatContext::InitInCtx(url, nullptr, options);

    // 2. 找到第一个视频流
    AVStream* stream = nullptr;
    int audio_index = -1;
    for(unsigned i = 0; i < fmt_ctx_->nb_streams; i++) {
        if (stream_type(i) == AVMEDIA_TYPE_AUDIO) {
            audio_index = i;
            stream = fmt_ctx_->streams[i];
            break;
        }
    }
    if(audio_index < 0 || !stream) {
        throw std::runtime_error("no audio stream found");
    }

    stream_ = Stream(stream);
    
    // 3. 初始化解码器上下文 codec_ codec_ctx_
    // 用stream初始化解码器上下文中已经复制了视频参数
    InitFromStream(stream_.raw(), true);
    Open(options);
}

FFmpegResult AudioDecoder::Decode(AVFrame* out_frame, int time_out) {

}

AVCodecContext* AudioDecoder::get() noexcept {
    return CodecContext::get();
}
const AVCodecContext* AudioDecoder::get() const noexcept {
    return CodecContext::get();
}
AVCodecContext* AudioDecoder::raw() noexcept {
    return CodecContext::raw();
}
const AVCodecContext* AudioDecoder::raw() const noexcept {
    return CodecContext::raw();
}

int AudioDecoder::sample_rate() const noexcept {
    return codec_ctx_ ? codec_ctx_->sample_rate : 0;
}
int AudioDecoder::channels() const noexcept {
    return codec_ctx_ ? codec_ctx_->ch_layout.nb_channels : 0;
}
AVSampleFormat AudioDecoder::sample_fmt() const noexcept {
    return codec_ctx_ ? codec_ctx_->sample_fmt : AV_SAMPLE_FMT_NONE;
}
int AudioDecoder::bit_rate() const noexcept {
    return codec_ctx_ ? codec_ctx_->bit_rate : 0;
}
AVChannelLayout AudioDecoder::channel_layout() const noexcept {
    return codec_ctx_ ? codec_ctx_->ch_layout : (AVChannelLayout){};
}
double AudioDecoder::duration() const noexcept {
    if (!fmt_ctx_ || !stream_.raw()) {
        return 0.0;
    }
    if (stream_.raw()->duration == AV_NOPTS_VALUE) {
        return static_cast<double>(fmt_ctx_->duration) / AV_TIME_BASE;    
    }

    return static_cast<double>(stream_.raw()->duration) * av_q2d(stream_.time_base());
}
int AudioDecoder::stream_idx() const noexcept {
    return stream_.raw() ? stream_.index() : -1;
}
AVRational AudioDecoder::time_base() const noexcept {
    if (!stream_.raw()) {
        return {0, 1};
    }
    return stream_.raw()->time_base;
}
/***********************************AudioEncoder*********************************/
AudioEncoder::AudioEncoder(const std::string& codec_name, int sample_rate, int channels, 
    AVSampleFormat sample_fmt, int bit_rate, bool is_hw, AVDictionary** options)  
    : CodecContext(codec_name, false), sample_rate_(sample_rate), channels_(channels), sample_fmt_(sample_fmt), bit_rate_(bit_rate)
{
    SetAudioCodecParameters(codec_ctx_, sample_rate, channels, sample_fmt, bit_rate);

    if (is_hw) {
        // SetHWDevice(AV_HWDEVICE_TYPE_CUDA);
    }
    
    // 打开编码器
    Open(options);
}

AudioEncoder::AudioEncoder(const AudioCodecParams& params, bool is_hw, AVDictionary** options)
    : CodecContext(params.codec_name, false), sample_rate_(params.sample_rate), channels_(params.channels), sample_fmt_(params.sample_fmt), bit_rate_(params.bit_rate)
{ 
    SetAudioCodecParameters(codec_ctx_, params.sample_rate, params.channels, params.sample_fmt, params.bit_rate);

    if (is_hw) {
        // SetHWDevice(AV_HWDEVICE_TYPE_CUDA);
    }
    
    // 打开编码器
    Open(options);
}

AVCodecContext* AudioEncoder::get() noexcept {
    return codec_ctx_;
}

AVCodecContext* AudioEncoder::raw() noexcept {
    return codec_ctx_;
}

const AVCodecContext* AudioEncoder::get() const noexcept {
    return codec_ctx_;
}

const AVCodecContext* AudioEncoder::raw() const noexcept {
    return codec_ctx_;
}

int AudioEncoder::sample_rate() const noexcept {
    return sample_rate_;
}

int AudioEncoder::channels() const noexcept {
    return channels_;
}

AVSampleFormat AudioEncoder::sample_fmt() const noexcept {
    return sample_fmt_;
}

int AudioEncoder::bit_rate() const noexcept {
    return bit_rate_;
}

AVChannelLayout AudioEncoder::channel_layout() const noexcept {
    return channel_layout_;
}

AVRational AudioEncoder::time_base() const noexcept {
    return time_base_;
}

}