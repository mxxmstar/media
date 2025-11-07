#include <iostream>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}

int main() {
    // 打印FFmpeg版本信息
    std::cout << "FFmpeg Version: " << av_version_info() << std::endl;
    
    // 注册所有组件（在较新版本中已弃用，但为了兼容性仍可使用）
    #if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(58, 9, 100)
    av_register_all();
    #endif
    
    std::cout << "Available encoders:" << std::endl;
    
    // 遍历并打印可用的编码器
    const AVCodec* codec = NULL;
    void* i = 0;
    while ((codec = av_codec_iterate(&i))) {
        if (av_codec_is_encoder(codec)) {
            std::cout << "- " << codec->name << " (" << codec->long_name << ")" << std::endl;
        }
    }
    
    std::cout << "\nAvailable muxers:" << std::endl;
    
    // 遍历并打印可用的复用器
    const AVOutputFormat* muxer = NULL;
    void* j = 0;
    while ((muxer = av_muxer_iterate(&j))) {
        std::cout << "- " << muxer->name << " (" << muxer->long_name << ")" << std::endl;
    }
    
    std::cout << "\nFFmpeg demo completed successfully!" << std::endl;
    
    return 0;
}