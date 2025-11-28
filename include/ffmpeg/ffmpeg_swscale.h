#pragma once

extern "C" {
#include "libswscale/swscale.h"
#include "libavutil/frame.h"
#include "libavutil/pixfmt.h"
#include "libavutil/channel_layout.h"
#include "libavutil/samplefmt.h"
#include "libswresample/swresample.h"
}

#include <stdexcept>
#include <memory>
#include <vector>

#include <functional>
#if defined(__cpp_lib_function_ref) && __cpp_lib_function_ref >= 202202L
#include <functional>

// 使用标准库中的function_ref
template<typename F>
using function_ref = std::function_ref<F>;
#elif defined(__cplusplus) && __cplusplus >= 202302L
template<typename F>
using function_ref = std::function<F>;
#else
#ifdef MCPPLIB
using function_ref = FunctionRef<F>;
#else
template<typename F>
using function_ref = std::function<F>;
#endif
#endif

namespace FFmpeg {
/// @brief 视频缩放上下文
class CSwsContext {
public:
    using AllocFunc = function_ref<uint8_t*(size_t size)>;
    using FreeFunc = function_ref<void(uint8_t* ptr, size_t size)>;
        
    /// @brief 构造函数
    /// @param srcW 源宽度
    /// @param srcH 源高度
    /// @param srcFormat 源像素格式
    /// @param dstW 目标宽度
    /// @param dstH 目标高度
    /// @param dstFormat 目标像素格式
    /// @param flags 缩放标志
    /// @param srcRange 源范围
    /// @param dstRange 目标范围
    CSwsContext(int srcW, int srcH, AVPixelFormat srcFormat, 
                int dstW, int dstH, AVPixelFormat dstFormat,
                int flags = SWS_BICUBIC, int srcRange = 0, int dstRange = 0);
    /// @brief 析构函数
    ~CSwsContext();

    CSwsContext(const CSwsContext&) = delete;
    CSwsContext& operator=(const CSwsContext&) = delete;

    /// @brief 移动构造函数
    /// @param other 其他对象
    CSwsContext(CSwsContext&& other) noexcept;

    /// @brief 移动赋值运算符
    /// @param other 其他对象
    /// @return 引用
    CSwsContext& operator=(CSwsContext&& other) noexcept;
        
    /// @brief 缩放函数
    /// @param src 源帧
    /// @param dst 目标帧
    /// @return 缩放结果
    void Scale(const AVFrame* src, AVFrame* dst);
    
    /// @brief 创建目标帧，仅CPU缩放场景可以直接使用av_frame_get_buffer
    /// @param dstW 目标宽度
    /// @param dstH 目标高度
    /// @param dstFormat 目标像素格式
    /// @return 目标帧
    static AVFrame* CreateFrame(int dstW, int dstH, AVPixelFormat dstFormat, int align = 32);
    
    /// @brief 创建自定义目标帧，自定义分配器场景可以使用
    /// @param dstW 目标宽度
    /// @param dstH 目标高度
    /// @param dstFormat 目标像素格式
    /// @param alloc 分配函数
    /// @param free_func 释放函数
    /// @param align 对齐要求
    /// @return 目标帧
    static AVFrame* CreateFrameCustom(int dstW, int dstH, AVPixelFormat dstFormat, const AllocFunc& alloc, const FreeFunc& free_func, int align = 1);

    /// @brief 将输入的AVFrame缩放并转换格式，打包为std::vector<uint8_t>
    /// @param src 源帧
    /// @param dstW 目标宽度
    /// @param dstH 目标高度
    /// @param dstFmt 目标像素格式
    std::vector<uint8_t> ScaleToPacked(AVFrame* src, int dstW, int dstH, AVPixelFormat dstFmt);

    /// @brief 获取源宽度
    int srcW() const noexcept { return srcW_; }
    /// @brief 获取源高度
    int srcH() const noexcept { return srcH_; }
    /// @brief 获取目标宽度
    int dstW() const noexcept { return dstW_; }
    /// @brief 获取目标高度
    int dstH() const noexcept { return dstH_; }
    
protected:
    SwsContext* sws_ctx_ = nullptr;
    int srcW_ = 0;
    int srcH_ = 0;
    AVPixelFormat srcFormat_ = AV_PIX_FMT_NONE;
    int dstW_ = 0;
    int dstH_ = 0;
    AVPixelFormat dstFormat_ = AV_PIX_FMT_NONE;
    
};


class CSwrContext { 
public:
    using AllocFunc = function_ref<uint8_t*(size_t size)>;
    using FreeFunc = function_ref<void(uint8_t* ptr, size_t size)>;

    /// @brief 构造函数
    /// @param src_sample_rate 源采样率
    /// @param src_channels 源通道数
    /// @param src_sample_fmt 源样本格式
    /// @param src_ch_layout 源通道布局
    /// @param dst_sample_rate 目标采样率
    /// @param dst_channels 目标通道数
    /// @param dst_sample_fmt 目标样本格式
    /// @param dst_ch_layout 目标通道布局
    CSwrContext(int src_sample_rate, int src_channels, enum AVSampleFormat src_sample_fmt, const AVChannelLayout& src_ch_layout, 
                int dst_sample_rate, int dst_channels, enum AVSampleFormat dst_sample_fmt, const AVChannelLayout& dst_ch_layout);

    ~CSwrContext();
    CSwrContext(const CSwrContext&) = delete;
    CSwrContext& operator=(const CSwrContext&) = delete;

    CSwrContext(CSwrContext&& other) noexcept;
    CSwrContext& operator=(CSwrContext&& other) noexcept;

    /// @brief 重采样
    /// @param src 源帧 
    /// @param dst 目标帧
    void Resample(const AVFrame* src, AVFrame* dst);

    /// @brief 创建音频帧
    /// @param sample_rate 采样率
    /// @param nb_samples 采样数
    /// @param ch_layout 声道布局
    /// @param  sample_fmt 采样格式
    /// @param align 对齐要求
    /// @return 音频帧
    static AVFrame* CreateFrame(int sample_rate, int nb_samples, const AVChannelLayout& ch_layout, 
                                     enum AVSampleFormat sample_fmt, int align = 0);

    /// @brief 创建自定义音频帧，自定义分配器场景可以使用
    /// @param sample_rate 采样率
    /// @param nb_samples 采样数
    /// @param ch_layout 声道布局
    /// @param sample_fmt 采样格式
    /// @param alloc 分配函数
    /// @param free_func 释放函数
    /// @param align 对齐要求
    /// @return 音频帧
    static AVFrame* CreateFrameCustom(int sample_rate, int nb_samples, const AVChannelLayout& ch_layout,
                                          enum AVSampleFormat sample_fmt, const AllocFunc& alloc, 
                                          const FreeFunc& free_func, int align = 0);

    /// @brief 将输入的AVFrame重采样并转换格式，打包为std::vector<uint8_t>
    /// @param src 源帧
    /// @param dst_sample_rate 目标采样率
    /// @param dst_ch_layout 目标声道布局
    /// @param dst_sample_fmt 目标采样格式
    /// @return 重采样后的数据
    std::vector<uint8_t> ResampleToPacked(AVFrame* src, int dst_sample_rate,
                                                  const AVChannelLayout& dst_ch_layout,
                                                  enum AVSampleFormat dst_sample_fmt);
    /// @brief 获取源通道布局
    const AVChannelLayout& src_ch_layout() const noexcept { return src_ch_layout_; }
    /// @brief 获取源采样格式
    enum AVSampleFormat src_sample_fmt() const noexcept { return src_sample_fmt_; }
    /// @brief 获取源采样率
    int src_sample_rate() const noexcept { return src_sample_rate_; }
    /// @brief 获取目标声道布局
    const AVChannelLayout& dst_ch_layout() const noexcept { return dst_ch_layout_; }
    /// @brief 获取目标采样格式
    enum AVSampleFormat dst_sample_fmt() const noexcept { return dst_sample_fmt_; }
    /// @brief 获取目标采样率
    int dst_sample_rate() const noexcept { return dst_sample_rate_; }

private:
    SwrContext* swr_ctx_ = nullptr;
    AVChannelLayout src_ch_layout_;
    int src_channels_ = 0;
    enum AVSampleFormat src_sample_fmt_ = AV_SAMPLE_FMT_NONE;
    int src_sample_rate_ = 0;
    AVChannelLayout dst_ch_layout_;
    int dst_channels_ = 0;
    enum AVSampleFormat dst_sample_fmt_ = AV_SAMPLE_FMT_NONE;
    int dst_sample_rate_ = 0;
};
}