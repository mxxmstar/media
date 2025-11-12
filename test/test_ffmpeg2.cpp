#include <iostream>
#include "ffmpeg/ffmpeg_log.h"
extern "C" {

}
using namespace FFmpeg;
int main() {
    SimpleLogger& logger = SimpleLogger::GetInstance();
    
    // 配置日志属性
    LoggerConfig cfg;
    cfg.file_path = "ffmpeg.log";
    cfg.min_level = LogLevel::Debug;
    cfg.is_async = true;
    logger.SetConfig(cfg);
    logger.Init();

    LOG_DEBUG("hello world");
    LOG_INFO("hello world");
    
    // 程序结束前停止日志记录器
    logger.Stop();
    return 0;
}