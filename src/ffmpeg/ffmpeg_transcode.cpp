#include "ffmpeg_transcode.h"
#include <iostream>
namespace FFmpeg {
VideoTranscoder::VideoTranscoder(const std::string& in_url, const std::string& out_url, const VideoCodecParams& params, bool is_hw, AVDictionary** options) 
    : decoder_(std::make_unique<VideoDecoder>(in_url, is_hw, options)),
      encoder_(std::make_unique<VideoEncoder>(params, is_hw, options)),
      params_(params), in_url_(in_url), out_url_(out_url) { 
    if (!decoder_) {
        throw std::runtime_error("decoder is null");
    } else {
        std::cout << "decoder init success" << std::endl;
    }
    if (!encoder_) {
        throw std::runtime_error("encoder is null");
    } else {
        std::cout << "encoder init success" << std::endl;
    } 
    if (decoder_->width() != params.width || decoder_->height() != params.height || decoder_->pix_fmt() != params.pix_fmt) {
        csws_ctx_ = std::make_unique<CSwsContext>(
            decoder_->width(), decoder_->height(), decoder_->pix_fmt(),
            params.width, params.height, params.pix_fmt);       
        // 失败会抛异常
    }
    std::cout << "CSwsContext init success" << std::endl;
    fmt_ctx_ = std::make_unique<FormatContext>(FormatContext::CreateOutFmtCtx(out_url_, nullptr, options));
    std::cout << "FormatContext init success" << std::endl;
    
    std::cout << "Format context pointer: " << fmt_ctx_->get() << std::endl;
    std::cout << "Output format: " << (fmt_ctx_->get()->oformat ? fmt_ctx_->get()->oformat->name : "null") << std::endl;

    // 创建输出流
    AVStream* out_stream = Stream::CreateStream(fmt_ctx_->get());
    std::cout << "Output stream created, index: " << out_stream->index << std::endl;
    std::cout << "Copying encoder parameters to stream..." << std::endl;
    std::cout << "Encoder context: " << encoder_->get() << std::endl;
    std::cout << "Output stream: " << out_stream << std::endl;
    // 拷贝编码器参数到输出流
    int ret = avcodec_parameters_from_context(out_stream->codecpar, encoder_->get());
    if(ret < 0) {
        throw std::runtime_error("avcodec_parameters_from_context failed");
    }
    std::cout << "FormatContext copy encoder params success" << std::endl;
    // 记录输出流索引
    stream_index_ = out_stream->index;
    std::cout << "Stream index set to: " << stream_index_ << std::endl;
    // 打开输出文件
    std::cout << "Opening output file..." << std::endl;
    std::cout << "Output format flags: " << fmt_ctx_->get()->oformat->flags << std::endl;
    if (!(fmt_ctx_->raw()->oformat->flags & AVFMT_NOFILE)){
        ret = avio_open(&fmt_ctx_->raw()->pb, out_url_.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            throw std::runtime_error("avio_open failed");
        }
    } else {
        std::cout << "Output format has AVFMT_NOFILE flag, skipping avio_open" << std::endl;
    }
    std::cout << "FormatContext open success" << std::endl;
    // 写文件头
    ret = avformat_write_header(fmt_ctx_->get(), nullptr);
    if(ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
        av_strerror(ret, errbuf, sizeof(errbuf));
        std::cout << "avformat_write_header failed: " << errbuf << std::endl;
        throw std::runtime_error("avformat_write_header failed");
    }
    std::cout << "FormatContext write header success" << std::endl;
}

VideoTranscoder::~VideoTranscoder() noexcept {
    
}
FFmpegResult VideoTranscoder::Transcode() {
    Frame frame;    
    Packet pkt;

    while (true) {
        // 解码一帧
        AVFrame* dec_frame = frame.raw();
        FFmpegResult dec_ret = decoder_->Decode(dec_frame);
        if (dec_ret == FFmpegResult::ENDFILE) {
            // 输入文件解码完成
            break;
        }
        if (dec_ret != FFmpegResult::SUCCESS) {
            return dec_ret;
        }

        AVFrame* proc_frame = frame.raw();
        Frame scaled_frame;
        if (csws_ctx_) {
            // 转换像素格式
            scaled_frame.reset(av_frame_alloc());
            if (!scaled_frame.get()) {
                return FFmpegResult::ERROR;
            }

            // 设置目标参数，分配缓冲区
            scaled_frame.AllocVideoBuffer(params_.width, params_.height, params_.pix_fmt);
            
            // 转换
            csws_ctx_->Scale(dec_frame, scaled_frame.get());

            // 转换后的帧作为处理帧
            proc_frame = scaled_frame.get();

            // 复制时间戳
            proc_frame->pts = dec_frame->pts;
            proc_frame->pkt_dts = dec_frame->pkt_dts;
        } else {
            proc_frame = dec_frame;
        }

        // 确保时间戳有效
        if (proc_frame->pts == AV_NOPTS_VALUE) {
            // TODO: 添加日志
            std::cerr << "Frame has no PTS, using sequential PTS" << std::endl;
            static int64_t frame_count = 0;
            // 创建基于帧率的时间戳
            AVRational time_base = av_inv_q(av_d2q(decoder_->fps(), 1000000));
            proc_frame->pts = av_rescale_q(frame_count++, time_base, decoder_->time_base());
        }     

        // 编码一帧
        AVPacket* enc_pkt = pkt.raw();
        FFmpegResult enc_ret = encoder_->Encode(proc_frame, enc_pkt);
        if (enc_ret == FFmpegResult::TRUE) {
            // 成功编码一个包，写入文件
            enc_pkt->stream_index = stream_index_;
            // 确保编码后的时间戳有效
            if (enc_pkt->pts == AV_NOPTS_VALUE) {
                std::cerr << "Frame has no PTS" << std::endl;
            }
            // 重新缩放时间戳到输出格式的时间基
            av_packet_rescale_ts(enc_pkt, decoder_->time_base(), fmt_ctx_->get()->streams[stream_index_]->time_base);
            int ret = av_interleaved_write_frame(fmt_ctx_->get(), enc_pkt);
            if(ret < 0) {
                // EOF
                char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
                av_strerror(ret, errbuf, sizeof(errbuf));
                std::cerr << "av_interleaved_write_frame failed: " << errbuf << std::endl;
                return FFmpegResult::ERROR;
            }

            pkt.unref();
        } else if (enc_ret == FFmpegResult::SEND_AGAIN) {
            // 编码器缓冲区满，需要继续发送数据
            // 在转码场景中这种情况较少见，但需要处理
            continue;
        } else {
            // 错误或其他情况（包括ENDFILE）
            return enc_ret;
        }
    }

    // 刷新编码器获取剩余的数据包
    Packet flush_pkt;
    FFmpegResult flush_ret = encoder_->Flush(flush_pkt.raw());
    while (flush_ret == FFmpegResult::TRUE) {
        // 写入最后的包
        flush_pkt.raw()->stream_index = stream_index_;

        if (flush_pkt.raw()->pts == AV_NOPTS_VALUE) {
                std::cerr << "Flush packet has no PTS" << std::endl;
        }

        av_packet_rescale_ts(flush_pkt.raw(), decoder_->time_base(), fmt_ctx_->get()->streams[stream_index_]->time_base);
        // int ret = av_interleaved_write_frame(fmt_ctx_->get(), flush_pkt.raw());
        // FFmpegResult write_ret = WritePacket(flush_pkt.raw());
        // if(write_ret != FFmpegResult::SUCCESS) {
        //     FFmpeg::tools::av_err(FFmpegResultHelper::toInt(write_ret));
        //     return FFmpegResult::ERROR;
        // }

        flush_pkt.unref();
        // 继续刷新编码器获取剩余的数据包
        flush_ret = encoder_->Flush(flush_pkt.raw());
    }
    return FFmpegResult::TRUE;
}
}