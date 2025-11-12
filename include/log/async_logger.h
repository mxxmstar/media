#include <thread>
#include <optional>
#include <chrono>
#include <string>

#include "stringview.h"
#include "singleton.h"

namespace AsyncLogger {

    // 日志级别枚举   
enum class LogLevel {
    // Trace: 详细调试信息 
    Trace,
    // Debug: 调试信息        
    Debug,
    // Info: 一般信息
    Info,
    // Warning: 警告信息
    Warning,
    // Error: 错误信息
    Error,
    // Fatal: 致命错误信息
    Fatal    
};

inline const char* LogLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::Trace: return "Trace";
        case LogLevel::Debug: return "Debug";
        case LogLevel::Info: return "Info";
        case LogLevel::Warning: return "Warning";
        case LogLevel::Error: return "Error";
        case LogLevel::Fatal: return "Fatal";
        default: return "Unknown";
    }
}

/// @brief 日志的9个可选字段
struct LogItem {
    /// @brief 时间戳
    std::optional<std::string> timestamp;
    /// @brief 时间戳是否存在
    bool hasTime = false;

    /// @brief 进程id
    std::optional<std::uint32_t> process_id;
    /// @brief 进程id是否存在
    bool hasPid = false;

    /// @brief 线程id
    std::optional<std::uint64_t> thread_id;
    /// @brief 线程id是否存在
    bool hasTid = false;

    /// @brief 协程id
    std::optional<std::uint64_t> coroutine_id;
    /// @brief 协程id是否存在
    bool hasCid = false;

    /// @brief 日志级别
    std::optional<LogLevel> level;
    /// @brief 日志级别是否存在
    bool hasLevel = false;

    /// @brief 文件名
    std::optional<std::string> file;
    /// @brief 文件名是否存在
    bool hasFile = false;

    /// @brief 行号
    std::optional<std::uint32_t> line;
    /// @brief 行号是否存在
    bool hasLine = false;

    /// @brief 函数名
    std::optional<std::string> function;
    /// @brief 函数名是否存在
    bool hasFunction = false;
    
    /// @brief 日志信息
    std::optional<std::string> message;
    /// @brief 日志信息是否存在
    bool hasMessage = false;
};

class AsyncLoggerBase {
public:
    virtual ~AsyncLoggerBase() = default;
    
    virtual void log(const LogItem& item) = 0;

};

class LoggerFactory {
public:
    static std::unique_ptr<AsyncLoggerBase> createBoostLogger();
};


class Logger : public Singleton<Logger> {
    friend class Singleton<Logger>;
public:
    void setLogger(std::unique_ptr<AsyncLoggerBase> logger) {
        std::lock_guard<std::mutex> lock(mutex_);
        logger_ = std::move(logger);
    }

    AsyncLoggerBase* getLogger() {
        std::lock_guard<std::mutex> lock(mutex_);
        return logger_.get();
    }

private:
    Logger() = default;

    std::unique_ptr<AsyncLoggerBase> logger_;
    std::mutex mutex_;
};


} // namespace AsyncLogger