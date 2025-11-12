// #include "log.h"
// #include <chrono>
// #include <vector>
// #include <unordered_map>

// // 时间戳关键词
// const std::unordered_set<std::string> timeKeywords = {
//     "time", "timestamp", "ts", "datetime", "date_time", "date", "time_stamp"
// };

// // 进程ID关键词
// const std::unordered_set<std::string> processKeywords = {
//     "pid", "process_id", "processid", "process"
// };

// // 线程id关键词
// const std::unordered_set<std::string> threadKeywords = {
//     "tid", "thread_id", "threadid", "thread"
// };

// // 协程id关键词
// const std::unordered_set<std::string> coroutineKeywords = {
//     "cid", "coroutine_id", "coroutineid", "coroutine"
// };

// // 日志级别关键词
// const std::unordered_set<std::string> levelKeywords = {
//     "level", "log_level", "severity", "loglevel"
// };

// // 文件名关键词
// const std::unordered_set<std::string> fileKeywords = {
//     "file", "filename", "source_file", "src_file"
// };

// // 函数名关键词
// const std::unordered_set<std::string> functionKeywords = {
//     "function", "func", "method", "method_name"
// };

// // 行号关键词
// const std::unordered_set<std::string> lineKeywords = {
//     "line", "lineno", "line_number", "source_line", "src_line", "line_num"
// };

// // 消息关键词
// const std::unordered_set<std::string> messageKeywords = {
//     "message", "msg", "log_message", "logmsg", "log", "log_msg"
// };  



//  // 解析fmt字符串，设置各个字段
// LogFormat::LogFormat(const std::string& fmt){
//     std::vector<std::string> keywords;
//     std::string currentKeyword = "";
//     size_t total = fmt.size();
//     for(size_t i = 0; i < total; ++i) {
//         if(i < total && isalpha(fmt[i]) == false) {
//             // 字符串非空则将其放到数组中
//             if(!currentKeyword.empty()) {
//                 keywords.push_back(currentKeyword);
//                 currentKeyword.clear();
//             }
//             continue; // 跳过非字母字符
//         }

//         currentKeyword += tolower(static_cast<unsigned char>(fmt[i])); // 累加字母字符
//     }
//     // 如果最后一个关键词非空，也加入到数组中
//     if(!currentKeyword.empty()) {
//         keywords.push_back(currentKeyword); 
//     }

//     for(const auto& keyword : keywords) {
//         if(LogFormat::timeKeywords.count(keyword)) {
//             hasTime_ = true;
//         } else if(LogFormat::processKeywords.count(keyword)) {
//             hasPid_ = true;
//         } else if(LogFormat::threadKeywords.count(keyword)) {
//             hasTid_ = true;
//         } else if(LogFormat::coroutineKeywords.count(keyword)) {
//             hasCid_ = true;
//         } else if(LogFormat::levelKeywords.count(keyword)) {
//             hasLevel_ = true;
//         } else if(LogFormat::fileKeywords.count(keyword)) {
//             hasFile_ = true;
//         } else if(LogFormat::functionKeywords.count(keyword)) {
//             hasFunction_ = true;
//         } else if(LogFormat::lineKeywords.count(keyword)) {
//             hasLine_ = true;
//         } else if(LogFormat::messageKeywords.count(keyword)) {
//             hasMessage_ = true;
//         }
//     }
// }

// void LogFormat::registerKeywords(){

// }

// LogBase::LogBase(std::string& fmt) : format_(fmt){
    
// }



