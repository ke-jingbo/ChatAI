#include "../../include/util/mylog.h"

namespace mylog {

    std::mutex Logger::_mutex;
    std::shared_ptr<spdlog::logger> Logger::_logger = nullptr;

    void Logger::Init(const std::string& log_name, const std::string& log_path, spdlog::level::level_enum level) {
        if(Logger::_logger != nullptr) return;
        {
            std::lock_guard<std::mutex> lock(Logger::_mutex);
            if(Logger::_logger != nullptr) return;
            // 刷新日志等级
            spdlog::flush_on(level);
            // 创建线程 启用异步日志
            spdlog::init_thread_pool(32768, 1);
            // 创建日志
            if(log_path == "stdout") {
                Logger::_logger = spdlog::stdout_color_mt(log_name);
            }
            else {
                // 单个日志文件达到 10 MiB 后滚动，最多保留 3 个历史文件。
                constexpr std::size_t max_file_size = 10 * 1024 * 1024;
                constexpr std::size_t max_files = 3;
                Logger::_logger = spdlog::rotating_logger_mt(
                    log_name,
                    log_path,
                    max_file_size,
                    max_files
                );
            }
            // 设置日志格式
            // %H:%M:%S 时间
            // %-7l 日志等级    
            // %n 日志名称
            // %v 日志内容
            Logger::_logger->set_pattern("[%H:%M:%S] [%-7l] [%n] %v");
        }
    }

    std::shared_ptr<spdlog::logger> Logger::GetLogger() {
        return Logger::_logger;
    }
}
