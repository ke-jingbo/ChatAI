#include "ChatServer.h"

#include <gflags/gflags.h>
#include <jsoncpp/json/json.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <thread>

// 基础服务参数从命令行读取；Provider 和模型配置从 JSON 文件读取。
DEFINE_string(host, "0.0.0.0", "Address on which the server listens");
DEFINE_uint32(port, 8080, "Port on which the server listens");
DEFINE_string(log_path, "./logs", "Log file path, or stdout");
DEFINE_string(log_level, "info", "Log level: trace, debug, info, warn, error, critical, off");
DEFINE_string(db_name, "chat_server.db", "SQLite database file");
DEFINE_string(config, "./config.json", "Provider and model JSON configuration file");

namespace {

// 版本号以及由信号处理函数设置的退出标志。
constexpr const char *kVersion = "1.0.0";
volatile std::sig_atomic_t g_stop_requested = 0;

void HandleSignal(int) {
    g_stop_requested = 1;
}

// 在交给 gflags 解析前识别项目自定义的短选项 -h 和 -v。
bool HasArgument(int argc, char **argv, const std::string &short_option,
                 const std::string &long_option) {
    for(int i = 1; i < argc; ++i) {
        const std::string argument(argv[i]);
        if(argument == short_option || argument == long_option) return true;
    }
    return false;
}

void PrintVersion() {
    std::cout << "AIChatServer " << kVersion << '\n';
}

void PrintHelp(const char *program) {
    std::cout
        << "AIChatServer " << kVersion << "\n\n"
        << "Usage:\n"
        << "  " << program << " [options]\n\n"
        << "Options:\n"
        << "  -h, --help                 Show this help message\n"
        << "  -v, --version              Show the version\n"
        << "  --host=<address>           Listen address (default: 0.0.0.0)\n"
        << "  --port=<port>              Listen port (default: 8080)\n"
        << "  --log_path=<path>          Log file/directory or stdout (default: ./logs)\n"
        << "  --log_level=<level>        trace/debug/info/warn/error/critical/off\n"
        << "                             (default: info)\n"
        << "  --db_name=<path>           SQLite database file (default: chat_server.db)\n"
        << "  --config=<path>            Provider/model JSON file (default: ./config.json)\n\n"
        << "Examples:\n"
        << "  " << program << " --config=./config.json\n"
        << "  " << program
        << " --host=127.0.0.1 --port=9000 --log_path=stdout --config=./config.json\n\n"
        << "HTTP API:\n"
        << "  GET    /api/models                         List available models\n"
        << "  GET    /api/sessions                       List sessions\n"
        << "  POST   /api/session                        Create a session\n"
        << "  DELETE /api/session/{session_id}           Delete a session\n"
        << "  GET    /api/session/{session_id}/history   Get session history\n"
        << "  POST   /api/session/model                  Change the session model\n"
        << "  POST   /api/session/model_config           Change model parameters\n"
        << "  POST   /api/message                        Send a message\n"
        << "  POST   /api/message/async                  Send a message using SSE\n\n"
        << "Cloud API keys can be provided through api_key_env in config.json;\n"
        << "the value names an environment variable rather than containing a secret.\n";
}

// 读取 JSON 对象中的必填非空字符串，并生成便于定位的错误信息。
bool ReadRequiredString(const Json::Value &object, const char *key,
                        std::string *value, std::string *error) {
    if(!object.isMember(key) || !object[key].isString() || object[key].asString().empty()) {
        *error = std::string("missing or empty string field: ") + key;
        return false;
    }
    *value = object[key].asString();
    return true;
}

bool ContainsControlCharacter(const std::string &value) {
    for(unsigned char character : value) {
        if(character < 0x20) return true;
    }
    return false;
}

// 将命令行字符串转换为 spdlog 使用的日志等级。
bool ParseLogLevel(const std::string &level, spdlog::level::level_enum *result) {
    if(level == "trace") *result = spdlog::level::trace;
    else if(level == "debug") *result = spdlog::level::debug;
    else if(level == "info") *result = spdlog::level::info;
    else if(level == "warn" || level == "warning") *result = spdlog::level::warn;
    else if(level == "error" || level == "err") *result = spdlog::level::err;
    else if(level == "critical") *result = spdlog::level::critical;
    else if(level == "off") *result = spdlog::level::off;
    else return false;
    return true;
}

// 在访问文件或构造服务对象前检查全部命令行参数。
bool ValidateCommandLine(std::string *error) {
    if(FLAGS_host.empty() || ContainsControlCharacter(FLAGS_host)) {
        *error = "host must be non-empty and contain no control characters";
        return false;
    }
    if(FLAGS_port == 0 || FLAGS_port > 65535) {
        *error = "port must be in the range 1..65535";
        return false;
    }
    if(FLAGS_log_path.empty() || ContainsControlCharacter(FLAGS_log_path)) {
        *error = "log_path must be non-empty and contain no control characters";
        return false;
    }
    if(FLAGS_db_name.empty() || ContainsControlCharacter(FLAGS_db_name)) {
        *error = "db_name must be non-empty and contain no control characters";
        return false;
    }
    if(FLAGS_config.empty() || ContainsControlCharacter(FLAGS_config)) {
        *error = "config must be non-empty and contain no control characters";
        return false;
    }
    spdlog::level::level_enum unused;
    if(!ParseLogLevel(FLAGS_log_level, &unused)) {
        *error = "invalid log_level: " + FLAGS_log_level;
        return false;
    }
    return true;
}

// log_path 可以是 stdout、日志文件或日志目录；目录最终写入 chat_server.log。
bool ResolveLogPath(const std::string &path_string, std::string *resolved,
                    std::string *error) {
    if(path_string == "stdout") {
        *resolved = path_string;
        return true;
    }
    std::error_code code;
    std::filesystem::path path(path_string);
    // 路径第一次使用时可能尚不存在，先判断是否存在。
    const bool path_exists = std::filesystem::exists(path, code);
    if(code) {
        *error = "cannot inspect log_path " + path_string + ": " + code.message();
        return false;
    }
    // 只有已存在的路径才检查其实际文件类型。
    bool is_directory = false;
    if(path_exists) {
        is_directory = std::filesystem::is_directory(path, code);
        if(code) {
            *error = "cannot inspect log_path " + path_string + ": " + code.message();
            return false;
        }
    }
    // 不存在且没有扩展名的路径按目录处理，并生成日志文件。
    if(is_directory || (!path_exists && !path.has_extension())) {
        std::filesystem::create_directories(path, code);
        if(code) {
            *error = "cannot create log directory " + path_string + ": " + code.message();
            return false;
        }
        path /= "chat_server.log";
    }
    // 获取父目录
    const std::filesystem::path parent = path.parent_path();
    if(!parent.empty()) std::filesystem::create_directories(parent, code);
    if(code) {
        *error = "cannot create directory for " + path_string + ": " + code.message();
        return false;
    }
    *resolved = path.string();
    return true;
}

// 数据库文件可以位于尚未创建的目录中，启动时自动创建其父目录。
bool PrepareDatabasePath(const std::string &path_string, std::string *error) {
    std::error_code code;
    const std::filesystem::path parent = std::filesystem::path(path_string).parent_path();
    if(!parent.empty()) std::filesystem::create_directories(parent, code);
    if(code) {
        *error = "cannot create directory for " + path_string + ": " + code.message();
        return false;
    }
    return true;
}

// 将一个 Provider JSON 对象转换成 SDK 配置。
// 云端 Provider 优先使用 api_key；示例配置使用 api_key_env 避免保存明文密钥。
bool ParseProvider(const Json::Value &value, ai_chat_sdk::ProviderConfigs *providers,
                   std::set<std::string> *provider_names, std::string *error) {
    if(!value.isObject()) {
        *error = "each providers entry must be an object";
        return false;
    }

    std::string provider_name;
    if(!ReadRequiredString(value, "provider", &provider_name, error)) return false;
    if(!provider_names->insert(provider_name).second) {
        *error = "duplicate provider: " + provider_name;
        return false;
    }

    if(provider_name == "OllamaLLMProvider") {
        std::string name;
        std::string path;
        if(!ReadRequiredString(value, "name", &name, error) ||
           !ReadRequiredString(value, "path", &path, error)) return false;
        if(path.rfind("http://", 0) != 0 && path.rfind("https://", 0) != 0) {
            *error = "OllamaLLMProvider path must start with http:// or https://";
            return false;
        }
        const std::string description = value.get("desc", "").asString();
        providers->push_back(std::make_shared<ai_chat_sdk::LocalConfig>(
            provider_name, name, path, description));
        return true;
    }

    if(provider_name != "DeepseekProvider" && provider_name != "MimoProvider" &&
       provider_name != "KimiProvider") {
        *error = "unsupported provider: " + provider_name;
        return false;
    }

    std::string api_key;
    if(value.isMember("api_key") && value["api_key"].isString()) {
        api_key = value["api_key"].asString();
    }
    if(api_key.empty() && value.isMember("api_key_env") && value["api_key_env"].isString()) {
        const std::string environment_name = value["api_key_env"].asString();
        if(environment_name.empty() || ContainsControlCharacter(environment_name)) {
            *error = "invalid api_key_env for " + provider_name;
            return false;
        }
        const char *environment_value = std::getenv(environment_name.c_str());
        if(environment_value != nullptr) api_key = environment_value;
        if(api_key.empty()) {
            *error = "environment variable " + environment_name +
                     " is not set for " + provider_name;
            return false;
        }
    }
    if(api_key.empty()) {
        *error = provider_name + " requires api_key or api_key_env";
        return false;
    }
    providers->push_back(
        std::make_shared<ai_chat_sdk::APIConfig>(provider_name, api_key));
    return true;
}

// 解析模型及其推理参数，并验证引用的 Provider 已经配置。
bool ParseModel(const Json::Value &value, const std::set<std::string> &provider_names,
                ai_chat_sdk::Models *models, std::set<std::string> *model_names,
                std::string *error) {
    if(!value.isObject()) {
        *error = "each models entry must be an object";
        return false;
    }

    std::string name;
    std::string provider;
    if(!ReadRequiredString(value, "name", &name, error) ||
       !ReadRequiredString(value, "provider", &provider, error)) return false;
    if(!model_names->insert(name).second) {
        *error = "duplicate model: " + name;
        return false;
    }
    if(provider_names.find(provider) == provider_names.end()) {
        *error = "model " + name + " references an unconfigured provider: " + provider;
        return false;
    }

    ai_chat_sdk::ModelConfig model_config;
    if(value.isMember("config")) {
        const Json::Value &config = value["config"];
        if(!config.isObject()) {
            *error = "config for model " + name + " must be an object";
            return false;
        }
        if(config.isMember("temperature")) {
            if(!config["temperature"].isNumeric()) {
                *error = "temperature for model " + name + " must be numeric";
                return false;
            }
            model_config._temperature = config["temperature"].asDouble();
        }
        if(config.isMember("max_tokens")) {
            if(!config["max_tokens"].isInt() && !config["max_tokens"].isUInt()) {
                *error = "max_tokens for model " + name + " must be an integer";
                return false;
            }
            model_config._max_tokens = config["max_tokens"].asInt();
        }
        if(config.isMember("think")) {
            if(!config["think"].isBool()) {
                *error = "think for model " + name + " must be boolean";
                return false;
            }
            model_config._think = config["think"].asBool();
        }
        if(config.isMember("reasoning_effort")) {
            if(!config["reasoning_effort"].isString()) {
                *error = "reasoning_effort for model " + name + " must be a string";
                return false;
            }
            model_config._reasoning_effort = config["reasoning_effort"].asString();
        }
    }

    const bool is_mimo_provider = provider == "MimoProvider";
    const double max_temperature = is_mimo_provider ? 1.5 : 2.0;
    const int max_tokens = is_mimo_provider ? 131072 : 393216;
    const char *temperature_range = is_mimo_provider ? "0..1.5" : "0..2";
    if(model_config._temperature < 0.0 || model_config._temperature > max_temperature) {
        *error = "temperature for model " + name + " must be in the range " +
                 temperature_range;
        return false;
    }
    if(model_config._max_tokens <= 0 || model_config._max_tokens > max_tokens) {
        *error = "max_tokens for model " + name + " must be in the range 1.." +
                 std::to_string(max_tokens);
        return false;
    }
    static const std::set<std::string> reasoning_levels = {"low", "medium", "high", "max"};
    if(reasoning_levels.find(model_config._reasoning_effort) == reasoning_levels.end()) {
        *error = "invalid reasoning_effort for model " + name;
        return false;
    }

    models->emplace_back(name, provider, value.get("desc", "").asString(), model_config);
    return true;
}

// 加载完整配置文件。先解析 Provider，再解析依赖 Provider 的模型。
bool LoadJsonConfiguration(const std::string &path,
                           ai_chat_server::ChatServerConfig *server_config,
                           std::string *error) {
    std::ifstream input(path);
    if(!input.is_open()) {
        *error = "cannot open configuration file: " + path;
        return false;
    }

    Json::Value root;
    Json::CharReaderBuilder builder;
    builder["collectComments"] = false;
    std::string parse_errors;
    if(!Json::parseFromStream(builder, input, &root, &parse_errors)) {
        *error = "invalid JSON in " + path + ": " + parse_errors;
        return false;
    }
    if(!root.isObject() || !root["providers"].isArray() || !root["models"].isArray()) {
        *error = "configuration must contain providers and models arrays";
        return false;
    }
    if(root["providers"].empty() || root["models"].empty()) {
        *error = "providers and models arrays must not be empty";
        return false;
    }

    std::set<std::string> provider_names;
    for(const auto &provider : root["providers"]) {
        if(!ParseProvider(provider, &server_config->_provider_configs,
                          &provider_names, error)) return false;
    }

    std::set<std::string> model_names;
    for(const auto &model : root["models"]) {
        if(!ParseModel(model, provider_names, &server_config->_models,
                       &model_names, error)) return false;
    }
    return true;
}

}  // namespace

int main(int argc, char **argv) {
    // 帮助和版本信息不依赖配置文件，应当优先响应。
    if(HasArgument(argc, argv, "-h", "--help")) {
        PrintHelp(argv[0]);
        return 0;
    }
    if(HasArgument(argc, argv, "-v", "--version")) {
        PrintVersion();
        return 0;
    }

    gflags::SetUsageMessage("AIChatServer - HTTP server for the ChatAI SDK");
    gflags::SetVersionString(kVersion);
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    // 先完成参数、目录和 JSON 检查，再创建可能启动线程的服务对象。
    std::string error;
    if(!ValidateCommandLine(&error)) {
        std::cerr << "Configuration error: " << error << '\n';
        return 2;
    }
    std::string resolved_log_path;
    if(!ResolveLogPath(FLAGS_log_path, &resolved_log_path, &error) ||
       !PrepareDatabasePath(FLAGS_db_name, &error)) {
        std::cerr << "Configuration error: " << error << '\n';
        return 2;
    }

    ai_chat_server::ChatServerConfig config;
    config._host = FLAGS_host;
    config._port = static_cast<uint16_t>(FLAGS_port);
    config._log_path = resolved_log_path;
    config._db_name = FLAGS_db_name;
    if(!ParseLogLevel(FLAGS_log_level, &config.log_level) ||
       !LoadJsonConfiguration(FLAGS_config, &config, &error)) {
        std::cerr << "Configuration error: " << error << '\n';
        return 2;
    }

    std::signal(SIGINT, HandleSignal);
    std::signal(SIGTERM, HandleSignal);

    try {
        ai_chat_server::ChatServer server(std::move(config));
        if(!server.Start()) {
            std::cerr << "Failed to start AIChatServer\n";
            return 1;
        }
        std::cout << "AIChatServer " << kVersion << " listening on "
                  << FLAGS_host << ':' << FLAGS_port << '\n'
                  << "Press Ctrl+C to stop.\n";
        // 信号处理函数只设置标志，资源释放和 Stop() 留在正常执行上下文完成。
        while(!g_stop_requested) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        server.Stop();
    }
    catch(const std::exception &exception) {
        std::cerr << "AIChatServer terminated: " << exception.what() << '\n';
        return 1;
    }
    catch(...) {
        std::cerr << "AIChatServer terminated: unknown exception\n";
        return 1;
    }

    return 0;
}
