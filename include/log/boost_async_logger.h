#pragma once

#include "async_logger.h"
#include "log_queue.h"


#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/console.hpp>

#include <thread>
#include <atomic>
#include <sstream>
#include <chrono>

class BoostAsyncLogger : public AsyncLogger::AsyncLoggerBase {
    using  LogLevel = AsyncLogger::LogLevel;    
public: 
    explicit BoostAsyncLogger(std::unique_ptr<LogQueue<AsyncLogger::LogItem>> queue);
    ~BoostAsyncLogger() override;
    void log(const AsyncLogger::LogItem& item) override;

    void addConsoleSink(LogLevel min_level = LogLevel::Trace, LogLevel max_level = LogLevel::Fatal);
    void addFileSink(const std::string& path, LogLevel min_level = LogLevel::Trace, LogLevel max_level = LogLevel::Fatal);
    void setConsoleColor(bool enable = true);
    // TODO:增加日志字段
    // void addFragment(const std::string& fragment);


private:
    void run();
    boost::log::trivial::severity_level convertToBoostLevel(LogLevel level);
    std::unique_ptr<LogQueue<AsyncLogger::LogItem>> queue_;
    std::atomic<bool> running_;
    std::thread worker_;
};