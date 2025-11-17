#include "ffmpeg_codec.h"
#include "ffmpeg_swscale.h"
namespace FFmpeg {
class VideoDecoder;
class VideoEncoder;
class AudioDecoder;
class AudioEncoder;
class VideoTranscoder{ 
public:
    /// @brief 创建一个视频转码器
    /// @param in_url 输入文件
    /// @param out_url 输出文件
    /// @param params 输出视频参数
    /// @param is_hw 是否使用硬件加速
    VideoTranscoder(const std::string& in_url, const std::string& out_url, const VideoCodecParams& params, bool is_hw, AVDictionary** options = nullptr);
    
    ~VideoTranscoder() noexcept;
    
    /// @brief 转码
    FFmpegResult Transcode();

private:
    std::unique_ptr<VideoDecoder> decoder_;
    std::unique_ptr<VideoEncoder> encoder_;
    std::unique_ptr<CSwsContext> csws_ctx_;
    std::unique_ptr<FormatContext> fmt_ctx_;
    
    VideoCodecParams params_;
    std::string in_url_;
    std::string out_url_;
    int stream_index_ = -1;
};



}