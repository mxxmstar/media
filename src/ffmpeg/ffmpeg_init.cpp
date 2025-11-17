#include "ffmpeg_init.h"

namespace FFmpeg{

FFmpegWrapper::FFmpegWrapper() : initialized_(false) {
}

FFmpegWrapper::~FFmpegWrapper() {
    if (initialized_) {
        Stop();
    }
}

void FFmpegWrapper::Init(bool is_console, const std::string& file_path, bool is_async) { 
    auto logger = LoggerManager::GetInstance().GetLogger();
    
    // 检查是否注册，未注册则注册默认的日志器
    if (!logger) {
        RegisterDefaultLogger(is_console, file_path, is_async);
    }
    MLOG_INFO("FFmpeg module initialized successfully");
    MLOG_INFO("Console output: " + std::string(is_console ? "enabled" : "disabled"));
    MLOG_INFO("File output: " + (file_path.empty() ? std::string("disabled") : file_path));
    MLOG_INFO("Async mode: " + std::string(is_async ? "enabled" : "disabled"));

    initialized_ = true;
}

void FFmpegWrapper::RegisterDefaultLogger(bool is_console, const std::string& file_path, bool is_async) {
    auto logger = LoggerManager::GetInstance().RegisterSingleton<SimpleLogger>();

    // 配置日志属性
    LoggerConfig cfg;
    cfg.to_console = is_console;
    cfg.to_file = !file_path.empty();
    cfg.file_path = file_path.empty() ? "ffmpeg.log" : file_path;
    cfg.min_level = LogLevel::Trace;
    cfg.is_async = is_async;
    
    // 初始化日志
    SimpleLogger::GetInstance().SetConfig(cfg);
    SimpleLogger::GetInstance().Init();
}
void FFmpegWrapper::Stop() {
    MLOG_INFO("FFmpeg module exited");
    auto logger = LoggerManager::GetInstance().GetLogger();
    if (logger) {
        logger->Stop();
    }
    initialized_ = false;
}

}