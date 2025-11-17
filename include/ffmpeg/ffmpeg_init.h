#pragma once
#include <string>
#include <memory>
#include "ffmpeg_log.h"
namespace FFmpeg {

class FFmpegWrapper {
public:
    FFmpegWrapper();
    ~FFmpegWrapper();
    void Init(bool is_console = true, const std::string& file_path = "", bool is_async = true);
    void RegisterDefaultLogger(bool is_console = true, const std::string& file_path = "", bool is_async = true);

    template<typename T, typename... Args>
    std::shared_ptr<T> RegisterLogger(Args&&... args)
    {
        return LoggerManager::GetInstance().RegisterLogger<T>(std::forward<Args>(args)...);
    }

    template<typename T>
    std::shared_ptr<ILog> RegisterSingletonLogger()
    {
        return LoggerManager::GetInstance().RegisterSingleton<T>();
    }

    void Stop();
private:
    bool initialized_;
};
}