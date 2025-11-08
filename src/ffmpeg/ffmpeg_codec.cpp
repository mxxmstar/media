#include "ffmpeg_codec.h"
#include "ffmpeg_avutil.h"
extern "C" {
//#include "libavdevice/avdevice.h"
//#include "libavutil/time.h"
#include <libavdevice/avdevice.h>
#include <libavutil/time.h>
}
#include <chrono>
namespace FFmpeg {

/*************************************AVPacket****************************************** */
Packet::Packet() {
    pkt_ = av_packet_alloc();
    if(!pkt_) {
        throw std::runtime_error("av_packet_alloc failed");
    }
}

Packet::~Packet() {
    // av_packet_free内部会判断*pkt_是否为空，构造函数中已经保证*pkt_不为空
    av_packet_free(&pkt_);
}

::AVPacket* Packet::get() noexcept {
    return pkt_;
}

::AVPacket* Packet::raw() noexcept {
    return pkt_;
}

const ::AVPacket* Packet::get() const noexcept {
    return pkt_;
}

const ::AVPacket* Packet::raw() const noexcept {
    return pkt_;
}

Packet::Packet(Packet&& other) noexcept {
    pkt_ = other.pkt_;
    other.pkt_ = nullptr;
}

Packet& Packet::operator=(Packet&& other) noexcept {
    if(this != &other) {
        av_packet_free(&pkt_);
        pkt_ = other.pkt_;
        other.pkt_ = nullptr;
    }
    return *this;
}

void Packet::unref() noexcept {
    av_packet_unref(pkt_);
}



/*************************************AVFrame*******************************************/
void Frame::AVFrameDeleter::operator()(AVFrame* frame) noexcept {
    if (frame) {
        av_frame_free(&frame);
    }
}
Frame::Frame() {
    frame_ = av_frame_alloc();
    if(!frame_) {
        throw std::runtime_error("av_frame_alloc failed");
    }
}

Frame::~Frame() {
    // av_frame_free内部会判断frame是否为空，构造函数中已经保证frame不为空
    av_frame_free(&frame_);
}

Frame::Frame(Frame&& other) noexcept {
    frame_ = other.frame_;
    other.frame_ = nullptr;
}

Frame& Frame::operator=(Frame&& other) noexcept {
    if(this != &other) {
        av_frame_free(&frame_);
        frame_ = other.frame_;
        other.frame_ = nullptr;
    }
    return *this;
}

void Frame::AllocVideoBuffer(int width, int height, AVPixelFormat format, int align) {
    // 极端判断
    if (!frame_) {
        frame_ = av_frame_alloc();
        if (!frame_) throw std::bad_alloc();
    }
    frame_->format = format;
    frame_->width = width;
    frame_->height = height;
    int ret = av_frame_get_buffer(frame_, align);
    if(ret < 0) {
        throw std::runtime_error("av_frame_get_buffer failed, ret: " + tools::av_err(ret));
    }
}

void Frame::AllocAudioBuffer(int sample_rate, int nb_samples, int channels, AVSampleFormat format, int align) { 
    // 极端判断
    if (!frame_) {
        frame_ = av_frame_alloc();
        if (!frame_) throw std::bad_alloc();
    }
    frame_->format = format;
    frame_->sample_rate = sample_rate;
    frame_->nb_samples = nb_samples;
    av_channel_layout_default(&frame_->ch_layout, channels);
    
    int ret = av_frame_get_buffer(frame_, align);
    if(ret < 0) {
        throw std::runtime_error("av_frame_get_buffer failed, ret: " + tools::av_err(ret));
    }
}

void Frame::free() noexcept {
    av_frame_free(&frame_);
    frame_ = nullptr;
}

AVFrame* Frame::take_ownership() noexcept {
    AVFrame* old = frame_;
    frame_ = nullptr;
    return old;
}

void Frame::reset(AVFrame* frame) noexcept {
    if (frame_) {
        av_frame_free(&frame_);
    }
    frame_ = frame;
}

void Frame::clone_from(const Frame& frame) {
    if (!frame.raw()) {
        free();
        return;
    }
    AVFrame* new_frame = av_frame_clone(frame.raw());
    if (!new_frame) {
        throw std::runtime_error("av_frame_clone failed");
    }
    reset(new_frame);
}

Frame::operator bool() const noexcept{
    return frame_ != nullptr;
}

AVFrame& Frame::operator*() noexcept {
    return *frame_;
}

const AVFrame& Frame::operator*() const noexcept {
    return *frame_;
}

AVFrame* Frame::operator->() noexcept {
    return frame_;
}

const AVFrame* Frame::operator->() const noexcept {
    return frame_;
}

AVFrame* Frame::CreateVideoFrame(int width, int height, AVPixelFormat format, int align) {
    AVFrame* frame = av_frame_alloc();
    if(!frame) {
        throw std::runtime_error("av_frame_alloc failed");
    }
    frame->format = format;
    frame->width = width;
    frame->height = height;
    int ret = av_frame_get_buffer(frame, align);
    if(ret < 0) {
        throw std::runtime_error("av_frame_get_buffer failed, ret: " + tools::av_err(ret));
    }
    return frame;
}
AVFrame* Frame::CreateAudioFrame(int sample_rate, int nb_samples, int channels, AVSampleFormat format, int align) {
    AVFrame* frame = av_frame_alloc();
    if(!frame) {
        throw std::runtime_error("av_frame_alloc failed");
    }
    frame->format = format;
    frame->sample_rate = sample_rate;
    frame->nb_samples = nb_samples;
    av_channel_layout_default(&frame->ch_layout, channels);
    int ret = av_frame_get_buffer(frame, align);
    if(ret < 0) {
        throw std::runtime_error("av_frame_get_buffer failed, ret: " + tools::av_err(ret));
    }
    return frame;
}

void Frame::FreeFrame(AVFrame* frame) noexcept {
    av_frame_free(&frame);
}

::AVFrame* Frame::get() noexcept {
    return frame_;
}

::AVFrame* Frame::raw() noexcept {
    return frame_;
}

const ::AVFrame* Frame::get() const noexcept {
    return frame_;
}

const ::AVFrame* Frame::raw() const noexcept {
    return frame_;
}




/*******************************AVCodecContext********************************************/
CodecContext::CodecContext(AVCodecID codec_id, bool is_decoder) {
    codec_ = const_cast<AVCodec*>(FindCodec(codec_id, is_decoder));
    codec_ctx_ = avcodec_alloc_context3(codec_);
    if(!codec_ctx_) {
        throw std::runtime_error("avcodec_alloc_context3() failed");
    }
}

CodecContext::CodecContext(const std::string& codec_name, bool is_decoder) {
    codec_ = const_cast<AVCodec*>(FindCodec(tools::codec_name2id(codec_name), is_decoder));
    codec_ctx_ = avcodec_alloc_context3(codec_);
    if(!codec_ctx_) {
        throw std::runtime_error("avcodec_alloc_context3() failed");
    }
}

CodecContext::CodecContext(const AVCodec* codec, bool is_decoder) {
    codec_ctx_ = avcodec_alloc_context3(codec);
    codec_ = const_cast<AVCodec*>(codec);
    if(!codec_ctx_) {
        throw std::runtime_error("avcodec_alloc_context3() failed");
    }
}

CodecContext::CodecContext(AVStream* stream, bool is_decoder) {
    codec_ = const_cast<AVCodec*>(FindCodec(stream->codecpar->codec_id, is_decoder));
    codec_ctx_ = avcodec_alloc_context3(codec_);
    if(!codec_ctx_) {
        throw std::runtime_error("avcodec_alloc_context3() failed");
    }

    // 解码时从 AVStream 中获取参数
    if (is_decoder) {
        avcodec_parameters_to_context(codec_ctx_, stream->codecpar);
    }
}

CodecContext::~CodecContext() {
    // 这里一定要判断，防止多次析构
    if(codec_ctx_) {
        avcodec_free_context(&codec_ctx_);
    }
}

void CodecContext::InitFromStream(AVStream* stream, bool is_decoder) {
    codec_ctx_ = CreateCodecCtxFromStream(stream, is_decoder, &codec_);
}

void CodecContext::FreeCodecCtx(AVCodecContext* codec_ctx) noexcept {
    avcodec_free_context(&codec_ctx);
}

AVCodecContext* CodecContext::CreateCodecCtxFromID(AVCodecID codec_id, bool is_decoder, AVCodec** codec) {
    // FindCodec() 保证返回非空
    const AVCodec* avcodec = FindCodec(codec_id, is_decoder);
    *codec = const_cast<AVCodec*>(avcodec);
    
    AVCodecContext* ctx = avcodec_alloc_context3(*codec);
    if(!ctx) {
        throw std::runtime_error("avcodec_alloc_context3() failed");
    }
    return ctx;
}

AVCodecContext* CodecContext::CreateCodecCtxFromName(const std::string& codec_name, bool is_decoder, AVCodec** codec) {
    AVCodecID codec_id = FFmpeg::tools::codec_name2id(codec_name);
    if(codec_id == AV_CODEC_ID_NONE) {
        throw std::runtime_error("codec_name2id failed: " + codec_name);
    }
    return CreateCodecCtxFromID(codec_id, is_decoder, codec);
}

AVCodecContext* CodecContext::CreateCodecCtxFromCodec(const AVCodec* codec, bool is_decoder) {
    AVCodecContext* ctx = avcodec_alloc_context3(codec);
    if(!ctx) {
        throw std::runtime_error("avcodec_alloc_context3() failed");
    }
    return ctx;
}

AVCodecContext* CodecContext::CreateCodecCtxFromStream(AVStream* stream, bool is_decoder, AVCodec** codec) {
    const AVCodec* avcodec = FindCodec(stream->codecpar->codec_id, is_decoder);
    *codec = const_cast<AVCodec*>(avcodec);
    AVCodecContext* ctx = avcodec_alloc_context3(*codec);
    if(!ctx) {
        throw std::runtime_error("avcodec_alloc_context3() failed");
    }
    if (is_decoder) {
        avcodec_parameters_to_context(ctx, stream->codecpar);
    }
    return ctx;
}

void CodecContext::Open(AVDictionary** options) {
    if (!codec_) {
        throw std::runtime_error("Codec has not been set");
    }

    if(avcodec_open2(codec_ctx_, codec_, options) < 0) {
        throw std::runtime_error("avcodec_open2 failed");
    }
}
FFmpegResult CodecContext::SendPacket(const ::AVPacket* pkt) noexcept {
 
   // 只考虑每个线程独占一个解码器的情况，不加锁
    int ret = avcodec_send_packet(codec_ctx_, pkt);
    if(ret == AVERROR(EAGAIN)) {
        return FFmpegResult::RECV_AGAIN;
    }
    if(ret == AVERROR_EOF) {
        return FFmpegResult::ENDFILE;
    }
    if(ret < 0) {
        return FFmpegResult::ERROR;
    }
    if (pkt != nullptr) {
        ++packets_send_;
    }
    return FFmpegResult::TRUE;
}

FFmpegResult CodecContext::SendNullPacket() noexcept {
    return SendPacket(nullptr);
}

FFmpegResult CodecContext::ReceiveFrame(AVFrame* frame) noexcept {
    if (frame == nullptr) {
        return FFmpegResult::ERROR;
    }

    int ret = avcodec_receive_frame(codec_ctx_, frame);
    if(ret == AVERROR(EAGAIN)) {
        return FFmpegResult::SEND_AGAIN;    // 需要更多输入的pkt
    }
    if(ret == AVERROR_EOF) {
        return FFmpegResult::ENDFILE;
    }
    if(ret < 0) {
        return FFmpegResult::ERROR;
    }

    ++frames_recv_;
    return FFmpegResult::TRUE;
}

FFmpegResult CodecContext::SendFrame(const AVFrame* frame) noexcept {
    int ret = avcodec_send_frame(codec_ctx_, frame);
    if(ret == AVERROR(EAGAIN)) {
        return FFmpegResult::RECV_AGAIN;    // 编码器内部缓冲区满
    }
    if(ret == AVERROR_EOF) {
        return FFmpegResult::ENDFILE;
    }
    if(ret < 0) {
        return FFmpegResult::ERROR;
    }

    if (frame != nullptr) {
        ++frames_send_;
    }
    return FFmpegResult::TRUE;
}

FFmpegResult CodecContext::ReceivePacket(AVPacket* pkt) noexcept { 
    if (pkt == nullptr) {
        return FFmpegResult::ERROR;
    }

    int ret = avcodec_receive_packet(codec_ctx_, pkt);
    if(ret == AVERROR(EAGAIN)) {
        return FFmpegResult::SEND_AGAIN;    // 需要更多输入的frame
    }
    if(ret == AVERROR_EOF) {
        return FFmpegResult::ENDFILE;
    }
    if(ret < 0) {
        return FFmpegResult::ERROR;
    }

    ++packets_recv_;
    return FFmpegResult::TRUE;
}

FFmpegResult CodecContext::Flush(AVPacket* pkt) noexcept {
    if (!pkt) {
        return FFmpegResult::ERROR;
    }

    int ret = avcodec_send_frame(codec_ctx_, nullptr);
    if (ret == 0) {
        return ReceivePacket(pkt);
    } else if (ret == AVERROR_EOF) {
        return FFmpegResult::ENDFILE;
    } else {
        return FFmpegResult::ERROR;
    }
}

CodecContext::CodecContext(CodecContext&& other) noexcept { 
    move_from(other); 
}
CodecContext& CodecContext::operator=(CodecContext&& other) noexcept {
    if (this != &other) {
        free(); // 先释放自己的资源
        move_from(other); 
    }
    return *this;
}

void CodecContext::move_from(CodecContext& other) noexcept {
    codec_ctx_ = other.codec_ctx_;
    codec_ = other.codec_;

    // 制空other
    other.codec_ctx_ = nullptr;
    other.codec_ = nullptr;
}

void CodecContext::free() noexcept {
    if(codec_ctx_) {
        FreeCodecCtx(codec_ctx_);
        codec_ctx_ = nullptr;
    }
}

const AVCodec* CodecContext::FindCodec(AVCodecID id, bool is_decoder) {
    const AVCodec* codec = nullptr;
    if(is_decoder) {
        codec = avcodec_find_decoder(id);
    } else {
        codec = avcodec_find_encoder(id);
    }
    
    if(!codec) {
        const char* type = is_decoder ? "decoder" : "encoder";
        throw std::runtime_error("avcodec_find_" + std::string(type) + std::string(" failed: ") + std::string(avcodec_get_name(id)));
    }
    return codec;
}

void CodecContext::SetVideoCodecParameters(AVCodecContext* codec_ctx, int width, int height, double fps, AVPixelFormat pix_fmt, int bit_rate) noexcept {
    codec_ctx->width = width;
    codec_ctx->height = height;
    codec_ctx->framerate = av_d2q(fps, AV_TIME_BASE);
    codec_ctx->time_base = av_inv_q(codec_ctx->framerate);
    codec_ctx->pix_fmt = pix_fmt;
    codec_ctx->bit_rate = bit_rate;
    // 设置默认视频质量参数
    codec_ctx->gop_size = static_cast<int>(fps);
    codec_ctx->max_b_frames = 1;
    codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
}

void CodecContext::SetVideoQualityCodecParameters(AVCodecContext* codec_ctx, int gop_size, int max_b_frames, int flags) noexcept {
    codec_ctx->gop_size = gop_size;
    codec_ctx->max_b_frames = max_b_frames;
    codec_ctx->flags |= flags;
}

AVCodecContext* CodecContext::get() noexcept {
    return codec_ctx_;
}
AVCodecContext* CodecContext::raw() noexcept {
    return codec_ctx_;
}
const AVCodecContext* CodecContext::get() const noexcept {
    return codec_ctx_;
}
const AVCodecContext* CodecContext::raw() const noexcept {
    return codec_ctx_;
}
#if 0
AVBufferRef* CodecContext::getHWDeviceContext() noexcept {
    return hw_device_ctx_.raw();
}

const AVBufferRef* CodecContext::getHWDeviceContext() const noexcept {
    return hw_device_ctx_.raw();
}

bool CodecContext::SetHWDevice(AVHWDeviceType type, const std::string& device_name = "") {
    // 错误后里面会抛异常
    hw_device_ctx_ = HWDeviceContext{type, device_name};
    if (!hw_device_ctx_.isValid() || codec_ctx_) {
        return false;
    }

    codec_ctx_->hw_device_ctx = av_buffer_ref(hw_device_ctx_.raw());
    if (!codec_ctx_->hw_device_ctx) {
        throw std::runtime_error("av_buffer_ref failed, can not to set hw_device_ctx");
    }

    for(int i = 0;; ++i) { 
        const AVCodecHWConfig* config = avcodec_get_hw_config(codec_, i);
        if (!config) {
            break;
        }
        if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
            config->device_type == hw_device_ctx_.getType()) {
            hw_pix_fmt_ = config->pix_fmt;
            break;
        }
    }

    if (hw_pix_fmt_ == AV_PIX_FMT_NONE) {
        throw std::runtime_error("HW device does not supported the codec.");
    }

    return true;
}

void CodecContext::SetDevicePool(std::shared_ptr<HWDevicePool> pool) {
    hw_device_pool_ = std::move(pool);
}

bool CodecContext::SetHWDeviceHandle(const DeviceHandle& handle) {
    if (!codec_ctx_) {
        throw std::runtime_error("Codec context is not initialized.");
    }

    if (!handle.valid()) {
        throw std::runtime_error("SetHWDeviceHandle: Invalid HW device handle.");
    }

    return SetHWDevice(handle.type(), handle.device_name());
    // // 给handle.hw_device_ctx增加引用计数，不拷贝实际数据
    // codec_ctx_->hw_device_ctx = av_buffer_ref(handle.hw_device_ctx());
    // if (!codec_ctx_->hw_device_ctx) {
    //     throw std::runtime_error("SetHWDeviceHandle: av_buffer_ref failed.");
    // }

    // // 确保 codec 支持 hw_device_ctx
    // auto* codec = codec_ctx_->codec;
    // hw_pix_fmt_ = AV_PIX_FMT_NONE;
    // for (int i = 0;; i++) {
    //     const AVCodecHWConfig* config = avcodec_get_hw_config(codec, i);
    //     if (!config) {
    //         break;
    //     }
    //     if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
    //         config->device_type == hw_device_ctx_.getType()) {
    //         hw_pix_fmt_ = config->pix_fmt;
    //         break;
    //     }
    // }

    // if (hw_pix_fmt_ == AV_PIX_FMT_NONE) {
    //     throw std::runtime_error("HW device does not supported the codec.");
    // }
    
    // return true;
}

bool CodecContext::TransferFrameToCPU(const Frame& src, Frame& dst) {
    if (!src.get()) {
        return false;
    }

    const AVFrame* in = src.get();

    // 非 hw frame 格式，直接深拷贝或 clone
    if (hw_pix_fmt_ == AV_PIX_FMT_NONE || in->format != hw_pix_fmt_)  {
        // 普通软件帧 / codec未设置 hw pixel format
        try { 
            dst.clone_from(src);
            return true;
        } catch (...) {
            return false;
        }
    }

    // 硬件帧转换成软件帧
    AVFrame* sw = av_frame_alloc();
    if (!sw) {
        return false;
    }

    int ret = av_hwframe_transfer_data(sw, src.get(), 0);
    if (ret < 0) {
        av_frame_free(&sw);
        std::fprintf(stderr, "Error transferring the data to system memory: %s\n", tools::av_err(ret).c_str());
        return false;
    }
    // 释放硬件帧，将指针指向sw
    dst.reset(sw);
    return true;
}
#endif

std::atomic<uint64_t>& CodecContext::getFrameRecv() noexcept {
    return frames_recv_;
}

std::atomic<uint64_t>& CodecContext::getPacketSend() noexcept {
    return packets_send_;
}

std::atomic<uint64_t>& CodecContext::getPacketRecv() noexcept {
    return packets_recv_;
}

std::atomic<uint64_t>& CodecContext::getFrameSend() noexcept {
    return frames_send_;
}

/************************************CodecSession***********************************/
// FFmpegResult CodecSession::Decode(::AVFrame* out_frame, int time_out,
//     ReadFrameFunc read_fc, SendPacketFunc send_fc, SendNullPacketFunc send_null_fc, RecvFrameFunc recv_fc, 
//     AVFormatContext* fmt_ctx, AVCodecContext* codec_ctx,
//     int target_stream_index) {
//     Packet pkt;
//     auto start = std::chrono::steady_clock::now();
//     int retry_count = 0;
   
//     while(true) {
//         int remain_ms = 0;
//         if (time_out > 0) {
//             auto now = std::chrono::steady_clock::now();
//             auto elapsed_ms = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count());
//             remain_ms = std::max(0, time_out - elapsed_ms);
//             if (remain_ms <= 0) {
//                 return FFmpegResult::TIMEOUT;    // 超时
//             }
//         }
       
//         // 将读取到的数据包发送给解复用器
//         FFmpegResult ret = read_fc(fmt_ctx, pkt.raw(), remain_ms);

//         // 成功拿到一个packet
//         if(ret == FFmpegResult::TRUE) {
//             if(pkt.get()->stream_index != target_stream_index) {
//                // 拿到的packet不是当前stream的，则跳过继续读取下一个packet
//                pkt.unref();
//                continue;
//             }

//             // 解复用将packet送入解码器
//             while (true) {
//                 FFmpegResult sret = send_fc(codec_ctx, pkt.raw());
//                 if (sret == FFmpegResult::TRUE) {
//                     // 发送成功
//                     pkt.unref();
                   
//                     // 尝试从解码器接收帧
//                     while (true) {
//                         FFmpegResult rret = recv_fc(codec_ctx, out_frame);
//                         if (FFmpegResult::SEND_AGAIN == rret) {
//                             break;   // 跳出内层 receive_frame 循环， 重试 send_packet
//                         } else {
//                             // EOF ERROR TRUE
//                             return rret;
//                         }   
//                     }
//                     // 因为send_packet成功，这个pkt被解码器消耗掉了，所以这里需要跳出外层 send_packet 循环 continue 继续读取下一个Frame
//                     break;   // 跳出外层 send_packet 循环 continue 继续读取下一个Frame
//                 } else if (sret == FFmpegResult::RECV_AGAIN) {
//                     retry_count = 0;
//                     // 解码器的输出缓冲已满，需要先从中取出Frame，解复用器不能再发packet了
//                     while(true) {

//                         // 尝试从解码器接收帧，腾出空间再send packet
//                         FFmpegResult rret = recv_fc(codec_ctx, out_frame);
//                         if (rret == FFmpegResult::TRUE) {
//                             // 成功取出一帧
//                             pkt.unref();
//                             return rret; 
//                         } else if (rret == FFmpegResult::SEND_AGAIN) {
//                             // 解码流程的逻辑处理出现了偏差，短暂等待后重试
//                             // TODO:添加日志
//                             // 检查超时
//                             if (time_out > 0) {
//                                 auto now = std::chrono::steady_clock::now();
//                                 auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
//                                 if (elapsed.count() >= time_out) {
//                                     // pkt 自动释放
//                                     return FFmpegResult::TIMEOUT;
//                                 }
//                             }

//                             if (++retry_count > DECODE_MAX_RETRY) {
//                                 pkt.unref();
//                                 return FFmpegResult::ERROR; // 认为是死循环状态
//                             }
//                             av_usleep(DECODE_SLEEP_US);
//                             break;   // 退出内层 receive_frame 循环， 重试 send_packet
//                         } else {
//                             // 错误 到达末尾
//                             pkt.unref();
//                             return rret; // ERROR EOF
//                         }
//                     }
//                     continue;    // 继续外层 send_packet
//                 } else if (sret == FFmpegResult::ENDFILE) {
//                     // codec 已经结束, 不能再send packet
//                     pkt.unref();
//                     return FFmpegResult::ENDFILE;
//                 } else {
//                     // 其他错误
//                     pkt.unref();
//                     return FFmpegResult::ERROR;
//                 }
//             }   // send packet loop
//             continue;    // 继续外层 read Frame loop
//         } else if (ret == FFmpegResult::ENDFILE) {
//             // demuxer EOF, 向解码器发送空包
//             pkt.unref();
//             FFmpegResult sret = send_null_fc(codec_ctx);
//             if (sret == FFmpegResult::ERROR) {
//                 return FFmpegResult::ERROR;
//             }

//             // 从解码器中取出剩余帧
//             while (true) { 
//                 FFmpegResult rret = recv_fc(codec_ctx, out_frame);
//                 if (rret == FFmpegResult::ENDFILE || rret == FFmpegResult::SEND_AGAIN) {
//                     return FFmpegResult::ENDFILE;
//                 } else {
//                     // ERR TRUE
//                     return rret;
//                 }
//             }
//         } else if(ret == FFmpegResult::TIMEOUT){
//             return FFmpegResult::TIMEOUT;
//         } else {
//             return FFmpegResult::ERROR;
//         }
//     }   // end outer read frame while
   
//     // 永远不会到达这里
//     return FFmpegResult::ERROR;
// }

// FFmpegResult CodecSession::Encode(::AVFrame* in_frame, AVPacket* out_pkt, int time_out,
//     SendFrameFunc send_fc, RecvPacketFunc recv_fc,
//     AVFormatContext* fmt_ctx, AVCodecContext* codec_ctx) {
//     auto start = std::chrono::steady_clock::now();
//     int retry_count = 0;

//     if (in_frame == nullptr) {
//         FFmpegResult flush_ret = send_fc(codec_ctx, nullptr);
//         if (flush_ret == FFmpegResult::ERROR) {
//             return FFmpegResult::ERROR;
//         }
//         return recv_fc(codec_ctx, out_pkt);
//     }
//     while (true) {
//         FFmpegResult sret = send_fc(codec_ctx, in_frame);
//         if (sret == FFmpegResult::TRUE) {
//             // send 成功后，清空输入
//             av_frame_unref(in_frame);

//             FFmpegResult rret = recv_fc(codec_ctx, out_pkt);
//             return rret; 
//         } else if (sret == FFmpegResult::RECV_AGAIN) {
//             // 队列满，先取出 packet
//             FFmpegResult rret = recv_fc(codec_ctx, out_pkt);
//             if (rret == FFmpegResult::SEND_AGAIN) {
//                 if (++retry_count > ENCODE_MAX_RETRY){
//                     return FFmpegResult::ERROR;
//                 }

//                 if (time_out > 0) {
//                     auto now = std::chrono::steady_clock::now();
//                     auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
//                     if (elapsed.count() >= time_out) {
//                         // pkt 自动释放
//                         return FFmpegResult::TIMEOUT;
//                     }
//                 }
//                 av_usleep(ENCODE_SLEEP_US);
//                 continue;
//             } else {
//                 return rret;
//             }
//         } else {
//             // EOF ERROR
//             return sret;
//         }
//     }
//     return FFmpegResult::ERROR;
// }

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
    if (!fmt_ctx_) {
        return 0.0;
    }
    return static_cast<double>(fmt_ctx_->duration) / AV_TIME_BASE;
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
    
    // 打开编码器
    Open(options);
}

VideoEncoder::VideoEncoder(const VideoCodecParams& params, bool is_hw, AVDictionary** options) 
    : CodecContext(params.codec_name, false), width_(params.width), height_(params.height), 
        fps_(params.fps), pix_fmt_(params.pix_fmt), bit_rate_(params.bit_rate) {
    // 配置编码器参数
    SetVideoCodecParameters(codec_ctx_, width_, height_, fps_, pix_fmt_, bit_rate_);
    
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

} // namespace FFmpeg