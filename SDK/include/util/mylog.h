#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/async.h>
#include <memory>

namespace mylog {

class Logger {
private:
    static std::shared_ptr<spdlog::logger> _logger;
    static std::mutex _mutex;

private:
    Logger();
    ~Logger();
    Logger(const Logger&) = delete;
    void operator=(const Logger&) = delete;

public:
    static void Init(const std::string& log_name, const std::string& log_path, spdlog::level::level_enum level);
    static std::shared_ptr<spdlog::logger> GetLogger();
};

}

#define TRACE(fmt, ...) mylog::Logger::GetLogger()->trace(std::string("[{:>10}:{:>4}]: ")+fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define DBG(fmt, ...) mylog::Logger::GetLogger()->debug(std::string("[{:>10}:{:>4}]: ")+fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define INFO(fmt, ...) mylog::Logger::GetLogger()->info(std::string("[{:>10}:{:>4}]: ")+fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define WARN(fmt, ...) mylog::Logger::GetLogger()->warn(std::string("[{:>10}:{:>4}]: ")+fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define ERR(fmt, ...) mylog::Logger::GetLogger()->error(std::string("[{:>10}:{:>4}]: ")+fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define CRI(fmt, ...) mylog::Logger::GetLogger()->critical(std::string("[{:>10}:{:>4}]: ")+fmt, __FILE__, __LINE__, ##__VA_ARGS__)