#include "ffmpeg_log.h"
#include <iostream>
#include <ctime>
#include <sstream>
#include <cstring>
#include <filesystem>   // C++ 17
#include <cstdarg>
namespace FFmpeg {
std::shared_ptr<ILog> LoggerManager::logger_ = nullptr;

void LoggerManager::SetLogger(std::shared_ptr<ILog> logger) {
    std::lock_guard<std::mutex> lock(mutex_);
    logger_ = std::move(logger);
}
std::shared_ptr<ILog> LoggerManager::GetLogger() {
    std::lock_guard<std::mutex> lock(mutex_);
    return logger_;
}
void LoggerManager::WriteLog(LogLevel level, const std::string msg, const char* file, const char* func, int line) {
    // ILog* logger = nullptr;
    // {
    //     std::lock_guard<std::mutex> lock(mutex_);
    //     logger = logger_.get();
    // }
    auto logger = LoggerManager::GetLogger();
    
    if (logger) {
        logger->WriteLog(level, msg, file, func, line);
    }
}

void LoggerManager::WriteLogFormat(LogLevel level, const char* fmt, const char* file, const char* func, int line, ...) {
    auto logger = LoggerManager::GetLogger();
    if (logger) {
        va_list args;
        va_start(args, line);

        va_list args_copy;
        va_copy(args_copy, args);
        int size = vsnprintf(nullptr, 0, fmt, args_copy);
        va_end(args);

        if (size > 0) {
            std::string formatted_msg(size , '\0');
            vsnprintf(&formatted_msg[0], size + 1, fmt, args);
            logger->WriteLog(level, formatted_msg, file, func, line);
        }
        va_end(args);
    }
}
void SimpleLogger::SetConfig(const LoggerConfig& cfg) {
    config_ = cfg;
}
void SimpleLogger::Init() {
    std::lock_guard<std::mutex> lock(log_io_mtx_);
    if (config_.to_file) {
        open_file();
    }

    if (config_.is_async) {
        running_ = true;
        worker_thread_ = std::thread([this] () {
            this->worker_loop();
        });
    }
}
void SimpleLogger::WriteLog(LogLevel l, const std::string msg, const char* file, const char* func, int line) { 
    if (l < config_.min_level) {
        return;
    }
    static const char* level_str[] = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};
    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    char timestamp[64];
    std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", std::localtime(&now));

    std::ostringstream oss;
    oss << "[" << timestamp << "] "
        << "[" << level_str[(int)l] << "] "
        << "[" << file_name_only(file) << ":" << line << " " << func << "] "
        << msg << "\n";
    std::string formatted_msg = oss.str();

    if (config_.is_async) {
        std::unique_lock<std::mutex> lock(log_queue_mtx_);
        log_queue_.push(formatted_msg);
        cv_.notify_one();
    } else {
        std::lock_guard<std::mutex> lock(log_io_mtx_);
        output_log(formatted_msg);
    }
}

void SimpleLogger::WriteLogFormat(LogLevel l, const char* fmt, const char* file, const char* func, int line, ...) {
    va_list args;
    // 指向line后面的可变参数
    va_start(args, line);
    // 格式化可变参数
    std::string formatted_msg = format_string(fmt, args);
    // 是否可变参数列表
    va_end(args);
    WriteLog(l, formatted_msg, file, func, line);
}
void SimpleLogger::Stop() {
    if (config_.is_async) {
        {
            std::unique_lock<std::mutex> lock(log_queue_mtx_);
            running_ = false;
        }
        cv_.notify_all();
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
    }
    if (log_file_.is_open()) {
        log_file_.close();
    }  
}

SimpleLogger::~SimpleLogger() {
    Stop();
}

const char* SimpleLogger::file_name_only(const char* path) {
    const char* file_name = strrchr(path, '/');
#ifdef _WIN32
    const char* file = strrchr(path, '\\');
    if (!file_name || (file && file > file_name)) file_name = file;
#endif
    return file_name ? file_name + 1 : path;
}

void SimpleLogger::open_file() {
    // 检查并创建目录
    std::filesystem::path log_path(config_.file_path);
    // 递归创建父目录
    if (log_path.has_parent_path()) {
        std::filesystem::create_directories(log_path.parent_path());
    }

    log_file_.open(config_.file_path, std::ios::app);
    if (!log_file_.is_open()) {
        std::cerr << "Failed to open log file: " << config_.file_path << std::endl;
    }
}

void SimpleLogger::output_log(const std::string& fmt_msg) {
    if (config_.to_console) {
        std::cout << fmt_msg;
    }
    if (config_.to_file && log_file_.is_open()) {
        check_rotation();
        log_file_ << fmt_msg;
        log_file_.flush();
    }
}

void SimpleLogger::check_rotation() {
    if (!config_.to_file) {
        return;
    }

    log_file_.seekp(0, std::ios::end);
    if (log_file_.tellp() < static_cast<std::streampos>(config_.max_file_size)) {
        return;
    }

    log_file_.close();

    for (int i = config_.max_file_count - 1; i >= 1; --i) {
        std::string old_file_path = config_.file_path + "." + std::to_string(i);
        std::string new_file_path = config_.file_path + "." + std::to_string(i + 1);
        std::remove(old_file_path.c_str());
        std::rename(old_file_path.c_str(), new_file_path.c_str());
    }

    std::string backup = config_.file_path + ".1";
    std::rename(config_.file_path.c_str(), backup.c_str());
    open_file();
}

void SimpleLogger::worker_loop() {
    std::unique_lock<std::mutex> lock(log_queue_mtx_);
    while(running_ || !log_queue_.empty()) {
        cv_.wait(lock, [this] { return !log_queue_.empty() || !running_; });
        while(!log_queue_.empty()) {
            std::string msg = std::move(log_queue_.front());
            log_queue_.pop();
            lock.unlock();
            {
                std::lock_guard<std::mutex> lock(log_io_mtx_);
                output_log(msg);
            }
            lock.lock();
        }
    }
}

std::string SimpleLogger::format_string(const char* fmt, va_list args) {
    va_list args_copy;
    va_copy(args_copy, args);
    // 不存储，直接计算可变参数的长度
    int size = vsnprintf(nullptr, 0, fmt, args_copy);
    va_end(args_copy);

    if (size <= 0) {
        return std::string(fmt);
    }

    std::string result(size, '\0');
    // 将格式化后的字符串存到result中
    vsnprintf(&result[0], size + 1, fmt, args);
    return result;
}

}