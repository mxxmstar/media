#include "boost_async_logger.h"
#include <map>
using namespace AsyncLogger;
BoostAsyncLogger::BoostAsyncLogger(std::unique_ptr<LogQueue<AsyncLogger::LogItem>> queue)
    : queue_(std::move(queue)), running_(true), worker_([this](){ run(); }) {
        
        boost::log::add_common_attributes();
}

BoostAsyncLogger::~BoostAsyncLogger() {
    running_ = false;
    queue_->stop();
    if(worker_.joinable()) {
        worker_.join();
    }
}

void BoostAsyncLogger::log(const AsyncLogger::LogItem& item) {
    queue_->push(item);
}

void BoostAsyncLogger::addConsoleSink(LogLevel min_level, LogLevel max_level) {
    auto console_sink = boost::log::add_console_log(std::cout);
    console_sink->set_formatter([](boost::log::record_view const& rec, boost::log::formatting_ostream& stream){
        stream << rec[boost::log::expressions::smessage];
    });

    auto boost_min_level = convertToBoostLevel(min_level);
    auto boost_max_level = convertToBoostLevel(max_level);
    // 控制台接收指定级别的日志
    console_sink->set_filter(boost::log::trivial::severity >= boost_min_level &&
        boost::log::trivial::severity <= boost_max_level);
}

void BoostAsyncLogger::addFileSink(const std::string& path, LogLevel min_level, LogLevel max_level) {
    auto file_sink = boost::log::add_file_log(path);
    file_sink->set_formatter([](boost::log::record_view const& rec, boost::log::formatting_ostream& stream){
        stream << rec[boost::log::expressions::smessage];
    });

    auto boost_min_level = convertToBoostLevel(min_level);
    auto boost_max_level = convertToBoostLevel(max_level);
    file_sink->set_filter(boost::log::trivial::severity >= boost_min_level && 
        boost::log::trivial::severity <= boost_max_level);
}

void BoostAsyncLogger::setConsoleColor(bool enable) {
    if (!enable) {
        return;
    }

    std::map<std::string, std::string> color_map = {
        {"Trace", "\033[0;36m"},    // 青色
        {"Debug", "\033[0;34m"},     // 蓝色
        {"Info", "\033[0;32m"},      // 绿色
        {"Warning", "\033[0;33m"},   // 黄色
        {"Error", "\033[0;31m"},     // 红色
        {"Fatal", "\033[0;35m"},     // 紫色
    };

    // 创建格式化器
    auto formatter = [color_map](boost::log::record_view const& rec,
        boost::log::formatting_ostream& stream) { 
        
        auto severity = boost::log::extract<boost::log::trivial::severity_level>("Severity", rec);
        std::string level_str = boost::log::trivial::to_string(severity.get());
        
        // 添加颜色
        auto color = color_map.find(level_str);
        if (color != color_map.end()) {
            stream << color->second;    // 设置颜色
        }

        // 添加日志内容
        stream << rec[boost::log::expressions::smessage];
        
        // 重置颜色
        stream << "\033[0m";
    };

    auto sink = boost::log::add_console_log(std::cout);
    sink->set_formatter(formatter);
}

void BoostAsyncLogger::run() {
    AsyncLogger::LogItem item;
    while(running_) {
        if(queue_->pop(item)) {
            std::ostringstream oss;
            if(item.hasTime) {
                oss << item.timestamp.value() << " ";
            }
            if(item.hasLevel) {
                oss << "["<< AsyncLogger::LogLevelToString(item.level.value()) << "] ";
            }
            if (item.hasPid) {
                oss << "[pid:" << *item.process_id << "] ";
            }
            if (item.hasTid) {
                oss << "[tid:" << *item.thread_id << "] ";
            }
            if (item.hasCid) {
                oss << "[cid:" << *item.coroutine_id << "] ";
            }
            if (item.hasFile) {
                oss << *item.file;
            }
            if (item.hasLine) {
                oss << ":" << *item.line << " ";
            }
            if (item.hasFunction) {
                oss << "(" << *item.function << ") ";
            }
            if (item.hasMessage) {
                oss << *item.message;
            }


            if (item.hasLevel) {
                switch (*item.level) {
                    case LogLevel::Trace: BOOST_LOG_TRIVIAL(trace) << oss.str(); break;
                    case LogLevel::Debug: BOOST_LOG_TRIVIAL(debug)   << oss.str(); break;
                    case LogLevel::Info:  BOOST_LOG_TRIVIAL(info)    << oss.str(); break;
                    case LogLevel::Warning:  BOOST_LOG_TRIVIAL(warning) << oss.str(); break;
                    case LogLevel::Error: BOOST_LOG_TRIVIAL(error)   << oss.str(); break;
                    case LogLevel::Fatal: BOOST_LOG_TRIVIAL(fatal)    << oss.str(); break;
                }
            } else {
                BOOST_LOG_TRIVIAL(info) << oss.str();
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

boost::log::trivial::severity_level BoostAsyncLogger::convertToBoostLevel(LogLevel level) {
    switch (level) {
        case LogLevel::Trace: return boost::log::trivial::trace;
        case LogLevel::Debug: return boost::log::trivial::debug;
        case LogLevel::Info: return boost::log::trivial::info;
        case LogLevel::Warning: return boost::log::trivial::warning;
        case LogLevel::Error: return boost::log::trivial::error;
        case LogLevel::Fatal: return boost::log::trivial::fatal;
        default: return boost::log::trivial::info;
    }
}