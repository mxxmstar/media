#include <iostream>
// #include "ffmpeg_log.h"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}
// using namespace FFmpeg;
int main() {
    // 1. 打印 FFmpeg 版本
    std::cout << "FFmpeg version: " << av_version_info() << std::endl;

    // 2. 打印各库版本号（十六进制）
    unsigned vc = avcodec_version();
    unsigned vf = avformat_version();
    unsigned vu = avutil_version();
    std::cout << "avcodec  0x" << std::hex << vc << std::dec << std::endl;
    std::cout << "avformat 0x" << std::hex << vf << std::dec << std::endl;
    std::cout << "avutil   0x" << std::hex << vu << std::dec << std::endl;

    std::cout << "✅ FFmpeg libraries linked successfully!" << std::endl;
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
    // return 0;
}