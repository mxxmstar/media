#include "ffmpeg_log.h"
#include "ffmpeg_init.h"
using namespace FFmpeg;
#include <iostream>
int main()
{
    // std::cout << "hello world" << std::endl;
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

    FFmpegWrapper ffmpeg;
    ffmpeg.Init(true, "ffmpeg_log.txt", true);
    MLOG_DEBUG("hello world");
    MLOG_INFO("hello world");
    ffmpeg.Stop();
}