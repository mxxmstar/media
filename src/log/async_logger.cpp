#include "log/async_logger.h"
#include "log/boost_async_logger.h"
namespace AsyncLogger {
std::unique_ptr<AsyncLoggerBase> LoggerFactory::createBoostLogger() {
    auto queue = std::make_unique<LogQueue<LogItem>>();
    return std::make_unique<BoostAsyncLogger>(std::move(queue));
}

}