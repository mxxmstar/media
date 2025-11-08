#pragma once
#include <iostream>
#include <string>
#include <cstdint>
#include <unordered_set>
// 支持格式: "time pid tid..." "[time][pid][tid]..." "time,pid,tid,..." "%time% %pid% %tid% ..."
class LogFormat {   
public:
    // 时间戳关键词
    static const std::unordered_set<std::string> timeKeywords;

    // 进程ID关键词
    static const std::unordered_set<std::string> processKeywords;

    // 线程id关键词
    static std::unordered_set<std::string> threadKeywords;

    // 协程id关键词
    static std::unordered_set<std::string> coroutineKeywords;

    // 日志级别关键词
    static std::unordered_set<std::string> levelKeywords;

    // 文件名关键词
    static std::unordered_set<std::string> fileKeywords;

    // 函数名关键词
    static std::unordered_set<std::string> functionKeywords;

    // 行号关键词
    static std::unordered_set<std::string> lineKeywords;

    // 消息关键词
    static std::unordered_set<std::string> messageKeywords;

    // 构造函数，初始化日志格式
    LogFormat(const std::string& fmt);

    /// @brief 注册日志关键词
    virtual void registerKeywords();
    
public:
    bool hasTime_ = false; // 时间戳
    bool hasPid_ = false; // 进程ID 
    bool hasTid_ = false; // 线程ID
    bool hasCid_ = false; // 协程ID
    bool hasLevel_ = false; // 日志级别
    bool hasLine_ = false; // 行号
    bool hasFile_ = false; // 文件名
    bool hasFunction_ = false; // 函数名
    bool hasMessage_ = false; // 日志消息
};

class LogBase {
public:
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

    // 构造函数，传入日志格式
    LogBase(std::string& fmt);
    LogBase() = delete;
    
    // 析构函数
    virtual ~LogBase() = default;

    // 初始化日志系统
    virtual void Init(std::string& fmt) = 0;
    
    // 设置日志格式
    virtual void SetLogFormat(const std::string& format) = 0;
   
    // 设置日志输出目标
    virtual void SetLogAppender(const std::string& appender = "") = 0;
   
    /// @brief 注册日志关键词
    virtual void registerKeywords(std::string& keyword);
   

    // 致命错误
    virtual void Fatal(const std::string& message) = 0;
    // 错误
    virtual void Error(const std::string& message) = 0;
    // 信息
    virtual void Info(const std::string& message) = 0;
    // 调试
    virtual void Debug(const std::string& message) = 0;
    // 警告
    virtual void Warning(const std::string& message) = 0;
    // 追踪
    virtual void Trace(const std::string& message) = 0;
protected:
    LogFormat format_; // 日志格式
    int64_t timeStamp_ = -1; // 时间戳
    int32_t processId_ = -1; // 进程ID 
    int32_t threadId_ = -1; // 线程ID
    int32_t coroutineId_ = -1; // 协程ID
    LogBase::LogLevel level_ = LogLevel::Debug; // 日志级别
    int32_t line_ = -1; // 行号
    std::string file_ = ""; // 文件名
    std::string function_ = ""; // 函数名
    std::string message_ = ""; // 日志消息

};