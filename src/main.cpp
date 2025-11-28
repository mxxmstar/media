#include <iostream>
#include "ffmpeg/ffmpeg_log.h"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
// #include "libavcodec/avcodec.h"
// #include "libavformat/avformat.h"
// #include "libavutil/avutil.h"
}
using namespace FFmpeg;
int main() {
    // // 打印FFmpeg版本信息
    // std::cout << "FFmpeg Version: " << av_version_info() << std::endl;
    
    // // 注册所有组件（在较新版本中已弃用，但为了兼容性仍可使用）
    // #if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(58, 9, 100)
    // av_register_all();
    // #endif
    
    // std::cout << "Available encoders:" << std::endl;
    
    // // 遍历并打印可用的编码器
    // const AVCodec* codec = NULL;
    // void* i = 0;
    // while ((codec = av_codec_iterate(&i))) {
    //     if (av_codec_is_encoder(codec)) {
    //         std::cout << "- " << codec->name << " (" << codec->long_name << ")" << std::endl;
    //     }
    // }
    
    // std::cout << "\nAvailable muxers:" << std::endl;
    
    // // 遍历并打印可用的复用器
    // const AVOutputFormat* muxer = NULL;
    // void* j = 0;
    // while ((muxer = av_muxer_iterate(&j))) {
    //     std::cout << "- " << muxer->name << " (" << muxer->long_name << ")" << std::endl;
    // }
    
    // std::cout << "\nFFmpeg demo completed successfully!" << std::endl;
    
    // auto& simple_logger = LoggerManager::GetInstance();
    
    // auto logger = LoggerManager::GetInstance().RegisterSingleton<SimpleLogger>();
    
    // // 配置日志属性
    // LoggerConfig cfg;
    // cfg.file_path = "ffmpeg.log";
    // cfg.min_level = LogLevel::Debug;
    // cfg.is_async = true;
    // SimpleLogger::GetInstance().SetConfig(cfg);
    // SimpleLogger::GetInstance().Init();

    // MLOG_DEBUG("hello world");
    // MLOG_INFO("hello world");

// 使用SimpleLogger单例直接配置和初始化
    SimpleLogger& logger = SimpleLogger::GetInstance();
    
    // 配置日志属性
    LoggerConfig cfg;
    cfg.file_path = "ffmpeg.log";
    cfg.min_level = LogLevel::Debug;
    cfg.is_async = true;
    logger.SetConfig(cfg);
    logger.Init();

    
    
    // 程序结束前停止日志记录器
    logger.Stop();

    return 0;
}