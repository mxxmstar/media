#pragma once
#include <string>
#include <memory>
#include <queue>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <fstream>
namespace FFmpeg {
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

enum class RotationType {
    None,
    Size,   // 按文件大小轮转
    Date    // 按日期轮转
};

struct LoggerConfig {
    bool is_async = false;
    bool to_console = true;
    bool to_file = false;
    std::string file_path = "ffmpeg.log";
    LogLevel min_level = LogLevel::Debug;
    RotationType rotation_type = RotationType::None;
    std::size_t max_file_size = 1024 * 1024 * 5;    // 轮转文件大小上限
    std::size_t max_file_count = 5; // 轮转文件数量上限
    std::size_t max_queue_size = 1000;
};

class ILog {
public:
    virtual ~ILog() = default;
    
    virtual void Init() = 0;

    virtual void WriteLog(LogLevel level, const std::string msg, const char* file, const char* func, int line) = 0;
    virtual void WriteLogFormat(LogLevel level, const char* fmt, const char* file, const char* func, int line, ...) = 0;
    virtual void Stop() = 0;
};

// 这里只实现管理一个日志器，多次注册的日志器会覆盖之前的日志器
class LoggerManager {
public:
    static LoggerManager& GetInstance() {
        static LoggerManager instance;
        return instance;
    }

    /// @brief 注册任意实现 ILog 的类型为日志器
    /// @tparam T 日志器类型，必须派生自 ILog
    /// @param args 日志器构造函数参数
    /// @return 注册的日志器智能指针
    template<typename T, typename... Args>
    std::shared_ptr<T> RegisterLogger(Args&&... args) {
        static_assert(std::is_base_of<ILog, T>::value, "T must be derived from ILog");
        std::lock_guard<std::mutex> lock(mutex_);
        std::shared_ptr<T> logger = std::make_shared<T>(std::forward<Args>(args)...);
        logger_ = logger;
        return logger;
    }

    template<typename T>
    std::shared_ptr<ILog> RegisterSingleton() {
        static_assert(std::is_base_of<ILog, T>::value, "T must be derived from ILog");
        std::lock_guard<std::mutex> lock(mutex_);
        // 必须实现 GetInstance() 静态成员函数
        std::shared_ptr<ILog> logger(std::shared_ptr<void>(), &T::GetInstance());
        logger_ = logger;
        return logger;
    }

    void SetLogger(std::shared_ptr<ILog> logger);
    std::shared_ptr<ILog> GetLogger();
    void WriteLog(LogLevel level, const std::string msg, const char* file, const char* func, int line);

    void WriteLogFormat(LogLevel level, const char* fmt, const char* file, const char* func, int line, ...);

private:
    LoggerManager() = default;
    static std::shared_ptr<ILog> logger_;
    std::mutex mutex_;
};

// -------- 宏封装 --------
#define MLOG_TRACE(msg) LoggerManager::GetInstance().WriteLog(LogLevel::Trace, msg, __FILE__, __FUNCTION__, __LINE__)
#define MLOG_DEBUG(msg) LoggerManager::GetInstance().WriteLog(LogLevel::Debug, msg, __FILE__, __FUNCTION__, __LINE__)
#define MLOG_INFO(msg)  LoggerManager::GetInstance().WriteLog(LogLevel::Info,  msg, __FILE__, __FUNCTION__, __LINE__)
#define MLOG_WARN(msg)  LoggerManager::GetInstance().WriteLog(LogLevel::Warning,  msg, __FILE__, __FUNCTION__, __LINE__)
#define MLOG_ERROR(msg) LoggerManager::GetInstance().WriteLog(LogLevel::Error, msg, __FILE__, __FUNCTION__, __LINE__)
#define MLOG_FATAL(msg) LoggerManager::GetInstance().WriteLog(LogLevel::Fatal, msg, __FILE__, __FUNCTION__, __LINE__)

#define MLOG_TRACE_F(fmt, ...) LoggerManager::GetInstance().WriteLogFormat(LogLevel::Trace, fmt, __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define MLOG_DEBUG_F(fmt, ...) LoggerManager::GetInstance().WriteLogFormat(LogLevel::Debug, fmt, __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define MLOG_INFO_F(fmt, ...)  LoggerManager::GetInstance().WriteLogFormat(LogLevel::Info,  fmt, __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define MLOG_WARN_F(fmt, ...)  LoggerManager::GetInstance().WriteLogFormat(LogLevel::Warning,  fmt, __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define MLOG_ERROR_F(fmt, ...) LoggerManager::GetInstance().WriteLogFormat(LogLevel::Error, fmt, __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define MLOG_FATAL_F(fmt, ...) LoggerManager::GetInstance().WriteLogFormat(LogLevel::Fatal, fmt, __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__)

class SimpleLogger : public ILog {
public:
    /// @brief 获取单例实例
    static SimpleLogger& GetInstance() {
        static SimpleLogger logger;
        return logger;
    }

    /// @brief 设置日志配置
    /// @param cfg 日志配置
    void SetConfig(const LoggerConfig& cfg);

    /// @brief 初始化日志器
    void Init() override;

    /// @brief 写入日志
    /// @param l 日志级别
    /// @param msg 日志消息
    /// @param file 文件名
    /// @param func 函数名
    /// @param line 行号
    void WriteLog(LogLevel l, const std::string msg, const char* file, const char* func, int line) override;
    
    /// @brief 写入格式化日志
    /// @param l 日志级别
    /// @param fmt 日志格式
    /// @param file 文件名
    /// @param func 函数名
    /// @param line 行号
    void WriteLogFormat(LogLevel l, const char* fmt, const char* file, const char* func, int line, ...) override;

    /// @brief 停止日志器
    void Stop() override;
private:
    SimpleLogger() = default;
    ~SimpleLogger() override;

    static const char* file_name_only(const char* path);

    void open_file();

    void output_log(const std::string& fmt_msg);

    void check_rotation();

    void worker_loop();
    
    /// @brief 将可变参数格式化为字符串
    /// @param fmt 格式字符串
    /// @param args 可变参数
    /// @return 格式化后的字符串
    std::string format_string(const char* fmt, va_list args);
private:
    LoggerConfig config_;
    std::ofstream log_file_;
    std::mutex log_io_mtx_;

    std::queue<std::string> log_queue_;
    std::mutex log_queue_mtx_;
    std::condition_variable cv_;
    std::thread worker_thread_;
    std::atomic<bool> running_{false};
};

#define LOG_TRACE(msg) SimpleLogger::GetInstance().WriteLog(LogLevel::Trace, msg, __FILE__, __FUNCTION__, __LINE__)
#define LOG_DEBUG(msg) SimpleLogger::GetInstance().WriteLog(LogLevel::Debug, msg, __FILE__, __FUNCTION__, __LINE__)
#define LOG_INFO(msg)  SimpleLogger::GetInstance().WriteLog(LogLevel::Info,  msg, __FILE__, __FUNCTION__, __LINE__)
#define LOG_WARN(msg)  SimpleLogger::GetInstance().WriteLog(LogLevel::Warning,  msg, __FILE__, __FUNCTION__, __LINE__)
#define LOG_ERROR(msg) SimpleLogger::GetInstance().WriteLog(LogLevel::Error, msg, __FILE__, __FUNCTION__, __LINE__)
#define LOG_FATAL(msg) SimpleLogger::GetInstance().WriteLog(LogLevel::Fatal, msg, __FILE__, __FUNCTION__, __LINE__)

#define LOG_TRACE_F(fmt, ...) SimpleLogger::GetInstance().WriteLogFormat(LogLevel::Trace, fmt, __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define LOG_DEBUG_F(fmt, ...) SimpleLogger::GetInstance().WriteLogFormat(LogLevel::Debug, fmt, __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define LOG_INFO_F(fmt, ...)  SimpleLogger::GetInstance().WriteLogFormat(LogLevel::Info,  fmt, __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define LOG_WARN_F(fmt, ...)  SimpleLogger::GetInstance().WriteLogFormat(LogLevel::Warning,  fmt, __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define LOG_ERROR_F(fmt, ...) SimpleLogger::GetInstance().WriteLogFormat(LogLevel::Error, fmt, __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define LOG_FATAL_F(fmt, ...) SimpleLogger::GetInstance().WriteLogFormat(LogLevel::Fatal, fmt, __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__)


}