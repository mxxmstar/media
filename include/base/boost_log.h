// #pragma once
// #include <boost/log/core.hpp>
// #include <boost/log/trivial.hpp>
// #include <boost/log/expressions.hpp>
// #include <boost/log/utility/setup/common_attributes.hpp>
// #include <boost/log/utility/setup/console.hpp>
// #include <boost/log/attributes/current_thread_id.hpp>
// #include <boost/log/attributes/current_process_id.hpp>
// #include <boost/date_time/posix_time/posix_time.hpp>
// #include <boost/log/sinks/text_file_backend.hpp>
// #include <boost/log/sinks/text_ostream_backend.hpp>
// #include <iostream>
// #include "log.h"
// #include <memory>

// namespace logging = boost::log;
// namespace expr = boost::log::expressions;
// namespace attrs = boost::log::attributes;

// using console_sink_t = boost::log::sinks::synchronous_sink<boost::log::sinks::text_ostream_backend>;
// using file_sink_t = boost::log::sinks::synchronous_sink<boost::log::sinks::text_file_backend>;

// class BoostLog : public LogBase {
// public:
//     void Init(std::string& fmt) override {
       
//     }

//     void SetLogFormat(const std::string& format) override {
        
//     }
//     void SetLogAppender(const std::string& appender) override {
//         std::lock_guard<std::mutex> lock(mtx_);
//         if (appender == "console") {
//             boost::log::add_console_log(std::clog);
//         } else {
//             boost::log::add_file_log(appender);
//         }
//     }
//     void Fatal(const std::string& message) override {
//         BOOST_LOG_TRIVIAL(fatal) << message;
//     }
//     void Error(const std::string& message) override {
//         BOOST_LOG_TRIVIAL(error) << message;
//     }
//     void Info(const std::string& message) override {
//         BOOST_LOG_TRIVIAL(info) << message;
//     }
//     void Debug(const std::string& message) override {
//         BOOST_LOG_TRIVIAL(debug) << message;
//     }
//     void Warning(const std::string& message) override {
//         BOOST_LOG_TRIVIAL(warning) << message;
//     }
//     void Trace(const std::string& message) override {
//         BOOST_LOG_TRIVIAL(trace) << message;
//     }
// private:
//     std::shared_ptr<console_sink_t> console_sink_;
//     std::shared_ptr<file_sink_t> file_sink_;
// };