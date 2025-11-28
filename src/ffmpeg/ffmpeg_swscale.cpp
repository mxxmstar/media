#include "ffmpeg_swscale.h"
extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/frame.h>
#include <libavcodec/avcodec.h>
}

namespace FFmpeg{
CSwsContext::CSwsContext(int srcW, int srcH, AVPixelFormat srcFormat, 
                         int dstW, int dstH, AVPixelFormat dstFormat,
                         int flags, int srcRange, int dstRange) :
    srcW_(srcW), srcH_(srcH), srcFormat_(srcFormat),
    dstW_(dstW), dstH_(dstH), dstFormat_(dstFormat) {
    sws_ctx_ = sws_getContext(srcW, srcH, srcFormat, dstW, dstH, dstFormat, flags, NULL, NULL, NULL);
    if (!sws_ctx_) {
        throw std::runtime_error("sws_getContext failed");
    }
}

CSwsContext::~CSwsContext() {
    if (sws_ctx_) {
        sws_freeContext(sws_ctx_);
        sws_ctx_ = nullptr;
    }
}

CSwsContext::CSwsContext(CSwsContext&& other) noexcept :
    srcW_(other.srcW_), srcH_(other.srcH_), srcFormat_(other.srcFormat_),
    dstW_(other.dstW_), dstH_(other.dstH_), dstFormat_(other.dstFormat_) {
    sws_ctx_ = other.sws_ctx_;
    other.sws_ctx_ = nullptr;
}

CSwsContext& CSwsContext::operator=(CSwsContext&& other) noexcept { 
    if (this != &other) {
        srcW_ = other.srcW_;
        srcH_ = other.srcH_;
        srcFormat_ = other.srcFormat_;
        dstW_ = other.dstW_;
        dstH_ = other.dstH_;
        dstFormat_ = other.dstFormat_;

        if (sws_ctx_) {
            sws_freeContext(sws_ctx_);
        }
        
        sws_ctx_ = other.sws_ctx_;
        other.sws_ctx_ = nullptr;
    }
    return *this;
}
void CSwsContext::Scale(const AVFrame* src, AVFrame* dst) {
    if (!sws_ctx_) {
        throw std::runtime_error("sws_ctx_ is null.");
    }
    if (!src || !dst) {
        throw std::runtime_error("src or dst is null.");
    } 
    auto ret = sws_scale(sws_ctx_, src->data, src->linesize, 0, src->height, dst->data, dst->linesize);
    if (ret < 0) {
        throw std::runtime_error("sws_scale failed");
    }
}

AVFrame* CSwsContext::CreateFrame(int dstW, int dstH, AVPixelFormat dstFormat, int align) {
    auto frame = av_frame_alloc();
    if (!frame) {
        throw std::runtime_error("av_frame_alloc failed");
    }
    frame->format = dstFormat;
    frame->width = dstW;
    frame->height = dstH;

    // 仅CPU缩放场景可以直接使用av_frame_get_buffer
    auto ret = av_frame_get_buffer(frame, align);  // 1或者32
    if (ret < 0) {
        throw std::runtime_error("av_frame_get_buffer failed");
    }
    return frame;
}

// FramePool：批量分配相同尺寸的 frame，支持重用。

// HwFrameAllocator：集成 av_hwframe_ctx_alloc() + av_hwframe_get_buffer()，封装硬件帧分配。

// FramePtr：封装 AVFrame* 为智能指针，构造时接收 FrameAllocator::allocate()。
AVFrame* CSwsContext::CreateFrameCustom(int dstW, int dstH, AVPixelFormat dstFormat, const AllocFunc& alloc, const FreeFunc& free_func, int align) { 
    if (!alloc || !free_func) {
        throw std::runtime_error("alloc or free_func is null");
    }

    AVFrame* frame = av_frame_alloc();
    if (!frame) {
        throw std::runtime_error("av_frame_alloc failed");
    }
    frame->format = dstFormat;
    frame->width = dstW;
    frame->height = dstH;

    int nb_bytes = av_image_get_buffer_size(dstFormat, dstW, dstH, align);
    if (nb_bytes < 0) {
        throw std::runtime_error("av_image_get_buffer_size failed");
    }

    uint8_t* data = alloc(nb_bytes);
    if (!data) {
        throw std::runtime_error("alloc failed");
    }
    int ret = av_image_fill_arrays(frame->data, frame->linesize, data, dstFormat, dstW, dstH, align);
    if (ret < 0) {
        free_func(data, nb_bytes);
        av_frame_free(&frame);
        throw std::runtime_error("av_image_fill_arrays failed");
    }

    struct FreeContext{
        FreeFunc fn;
        int sz;
    };

    FreeContext* ctx = new FreeContext{free_func, nb_bytes};
    AVBufferRef* buf = av_buffer_create(data, nb_bytes, [](void* opaque, uint8_t* data) {
        FreeContext* ctx = static_cast<FreeContext*>(opaque);
        if (ctx && ctx->fn) {
            ctx->fn(data, ctx->sz);
        }
        delete ctx;
    }, 
    ctx, 0);
    if (!buf) {
        av_frame_free(&frame);
        delete ctx;
        throw std::runtime_error("av_buffer_create failed");
    }
    frame->buf[0] = buf;
    return frame;
}

std::vector<uint8_t> CSwsContext::ScaleToPacked(AVFrame* src, int dstW, int dstH, AVPixelFormat dstFormat) { 
    // 创建目标帧
    AVFrame* dst = CreateFrame(dstW, dstH, dstFormat);
    // 执行缩放
    Scale(src, dst);
    // 计算目标缓冲区大小
    int nb = av_image_get_buffer_size(dstFormat, dstW, dstH, 1);
    if (nb < 0) {
        av_frame_free(&dst);
        throw std::runtime_error("av_image_get_buffer_size failed");
    }
    std::vector<uint8_t> packed(nb);
    int ret = av_image_copy_to_buffer(packed.data(), nb, dst->data, dst->linesize, dstFormat, dstW, dstH, 1);
    if (ret < 0) {
        av_frame_free(&dst);
        throw std::runtime_error("av_image_copy_to_buffer failed");
    }
    return packed;
}   


/***********************************CSwrContext****************************************/
CSwrContext::CSwrContext(int src_sample_rate, int src_channels, enum AVSampleFormat src_sample_fmt, const AVChannelLayout& src_ch_layout, 
                int dst_sample_rate, int dst_channels, enum AVSampleFormat dst_sample_fmt, const AVChannelLayout& dst_ch_layout)
    : src_channels_(src_channels), src_sample_fmt_(src_sample_fmt), src_sample_rate_(src_sample_rate),
    dst_channels_(dst_channels), dst_sample_fmt_(dst_sample_fmt), dst_sample_rate_(dst_sample_rate)
{
    // 复制声道布局
    int ret = av_channel_layout_copy(&src_ch_layout_, &src_ch_layout);
    if (ret < 0) {
        throw std::runtime_error("av_channel_layout_copy for source failed");
    }
    
    ret = av_channel_layout_copy(&dst_ch_layout_, &dst_ch_layout);
    if (ret < 0) {
        av_channel_layout_uninit(&src_ch_layout_);
        throw std::runtime_error("av_channel_layout_copy for destination failed");
    }

    ret = swr_alloc_set_opts2(&swr_ctx_, 
                              &src_ch_layout_, dst_sample_fmt_, dst_sample_rate_, 
                              &dst_ch_layout_, src_sample_fmt_, src_sample_rate_, 
                              0, nullptr);
    if (ret < 0 || !swr_ctx_) {
        av_channel_layout_uninit(&src_ch_layout_);
        av_channel_layout_uninit(&dst_ch_layout_);
        throw std::runtime_error("swr_alloc_set_opts2 failed");
    }
    
    // 初始化重采样上下文
    ret = swr_init(swr_ctx_);
    if (ret < 0) {
        swr_free(&swr_ctx_);
        av_channel_layout_uninit(&src_ch_layout_);
        av_channel_layout_uninit(&dst_ch_layout_);
        throw std::runtime_error("swr_init failed");
    }
}

CSwrContext::~CSwrContext() {
    swr_free(&swr_ctx_);
    av_channel_layout_uninit(&src_ch_layout_);
    av_channel_layout_uninit(&dst_ch_layout_);
}

CSwrContext::CSwrContext(CSwrContext&& other) noexcept : swr_ctx_(other.swr_ctx_), 
    src_ch_layout_(other.src_ch_layout_), src_channels_(other.src_channels_), src_sample_fmt_(other.src_sample_fmt_), src_sample_rate_(other.src_sample_rate_),
    dst_ch_layout_(other.dst_ch_layout_), dst_channels_(other.dst_channels_), dst_sample_fmt_(other.dst_sample_fmt_), dst_sample_rate_(other.dst_sample_rate_)
{
    swr_ctx_ = other.swr_ctx_;
    other.swr_ctx_ = nullptr;

    // 清除源对象的声道布局，因为所有权已经转移
    other.src_ch_layout_ = {};
    other.dst_ch_layout_ = {};
}

CSwrContext& CSwrContext::operator=(CSwrContext&& other) noexcept {
    if (this != &other) {
        swr_free(&swr_ctx_);
        av_channel_layout_uninit(&src_ch_layout_);
        av_channel_layout_uninit(&dst_ch_layout_);

        // 转移资源
        src_ch_layout_ = other.src_ch_layout_;
        src_channels_ = other.src_channels_;
        src_sample_fmt_ = other.src_sample_fmt_;
        src_sample_rate_ = other.src_sample_rate_;
        dst_ch_layout_ = other.dst_ch_layout_;
        dst_channels_ = other.dst_channels_;
        dst_sample_fmt_ = other.dst_sample_fmt_;
        dst_sample_rate_ = other.dst_sample_rate_;
        swr_ctx_ = other.swr_ctx_;
        other.swr_ctx_ = nullptr;
        
        // 清除源对象的声道布局
        other.src_ch_layout_ = {};
        other.dst_ch_layout_ = {};
    }
    return *this;
}

void CSwrContext::Resample(const AVFrame* src, AVFrame* dst) {
    if (!swr_ctx_) {
        throw std::runtime_error("swr_ctx_ is null");
    }
    if (!src || !dst) {
        throw std::runtime_error("src or dst is null");
    } 
    auto ret = swr_convert_frame(swr_ctx_, dst, src);
    if (ret < 0) {
        throw std::runtime_error("swr_convert_frame failed");
    }
}

AVFrame* CSwrContext::CreateFrame(int sample_rate, int nb_samples, const AVChannelLayout& ch_layout, enum AVSampleFormat sample_fmt, int align) {
    auto frame = av_frame_alloc();
    if (!frame) {
        throw std::runtime_error("av_frame_alloc failed");
    }
    frame->sample_rate = sample_rate;
    frame->nb_samples = nb_samples;
    frame->format = sample_fmt;

    int ret = av_channel_layout_copy(&frame->ch_layout, &ch_layout);
    if (ret < 0) {
        av_frame_free(&frame);
        throw std::runtime_error("av_channel_layout_copy failed");
    }

    // 仅CPU缩放场景可以直接使用av_frame_get_buffer
    ret = av_frame_get_buffer(frame, align);  // 1或者32
    if (ret < 0) {
        av_frame_free(&frame);
        throw std::runtime_error("av_frame_get_buffer failed");
    }
    return frame;
}

AVFrame* CSwrContext::CreateFrameCustom(int sample_rate, int nb_samples, const AVChannelLayout& ch_layout,
                                          enum AVSampleFormat sample_fmt, const AllocFunc& alloc, 
                                          const FreeFunc& free_func, int align)
{
    if (!alloc || !free_func) {
        throw std::runtime_error("alloc or free_func is null");
    }

    AVFrame* frame = av_frame_alloc();
    if (!frame) {
        throw std::runtime_error("av_frame_alloc failed");
    }
    frame->format = sample_fmt;
    frame->sample_rate = sample_rate;
    frame->nb_samples = nb_samples;

    int ret = av_channel_layout_copy(&frame->ch_layout, &ch_layout);
    if (ret < 0) {
        av_frame_free(&frame);
        throw std::runtime_error("av_channel_layout_copy failed");
    }

    int nb_bytes = av_samples_get_buffer_size(nullptr, ch_layout.nb_channels, nb_samples, sample_fmt, align);
    if (nb_bytes < 0) {
        av_frame_free(&frame);
        throw std::runtime_error("av_samples_get_buffer_size failed");
    }

    uint8_t* data = alloc(nb_bytes);
    if (!data) {
        av_frame_free(&frame);
        throw std::runtime_error("alloc failed");
    }
    ret = avcodec_fill_audio_frame(frame, ch_layout.nb_channels, sample_fmt, data, nb_bytes, align);
    if (ret < 0) {
        free_func(data, nb_bytes);
        av_frame_free(&frame);
        throw std::runtime_error("avcodec_fill_audio_frame failed");
    }

    struct FreeContext{
        FreeFunc fn;
        int sz;
    };

    FreeContext* ctx = new FreeContext{free_func, nb_bytes};
    AVBufferRef* buf = av_buffer_create(data, nb_bytes, [](void* opaque, uint8_t* data) {
        FreeContext* ctx = static_cast<FreeContext*>(opaque);
        if (ctx && ctx->fn) {
            ctx->fn(data, ctx->sz);
        }
        delete ctx;
    }, 
    ctx, 0);
    if (!buf) {
        av_frame_free(&frame);
        delete ctx;
        throw std::runtime_error("av_buffer_create failed");
    }
    frame->buf[0] = buf;
    return frame;
}
                                    
std::vector<uint8_t> CSwrContext::ResampleToPacked(AVFrame* src, int dst_sample_rate,
                                                  const AVChannelLayout& dst_ch_layout,
                                                  enum AVSampleFormat dst_sample_fmt)
{
    // 创建目标帧
    AVFrame* dst = CreateFrame(dst_sample_rate, src->nb_samples, dst_ch_layout, dst_sample_fmt);
    // 执行重采样
    Resample(src, dst);

    // 计算目标缓冲区大小
    int nb = av_samples_get_buffer_size(nullptr, dst_ch_layout.nb_channels, dst->nb_samples, dst_sample_fmt, 1);
    if (nb < 0) {
        av_frame_free(&dst);
        throw std::runtime_error("av_image_get_buffer_size failed");
    }

    // 创建并填充结果向量
    std::vector<uint8_t> packed(nb);

    int ret = 0;
    if (av_sample_fmt_is_planar(dst_sample_fmt)) { 
        // 平面格式，需要将各平面数据交织
        // ret = av_samples_copy(&packed.data(), dst->data, 0, 0, dst->nb_samples, 
        //                       dst_ch_layout.nb_channels, dst_sample_fmt);
        uint8_t* dst_data[AV_NUM_DATA_POINTERS] = {0};  // AV_NUM_DATA_POINTERS通常为8
        dst_data[0] = packed.data();
        ret = av_samples_copy(dst_data, dst->data, 0, 0, dst->nb_samples, 
                              dst_ch_layout.nb_channels, dst_sample_fmt);
    } else {
        memcpy(packed.data(), dst->data[0], nb);
        ret = 0;
    }
    
    if (ret < 0) {
        av_frame_free(&dst);
        throw std::runtime_error("av_image_copy_to_buffer failed");
    }
    av_frame_free(&dst);
    return packed;
}

}