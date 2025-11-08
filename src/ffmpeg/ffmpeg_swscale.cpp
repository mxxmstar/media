#include "ffmpeg_swscale.h"
extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/frame.h>
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
        throw std::runtime_error("sws_getContext failed");
    }
    if (!src || !dst) {
        throw std::runtime_error("src or dst is null");
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
    AVFrame* dst = CreateFrame(dstW, dstH, dstFormat);
    Scale(src, dst);
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

}