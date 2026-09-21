#pragma once
#include <ai_chat_sdk/ChatSDK.h>
#include <httplib.h>
#include <ai_chat_sdk/util/mylog.h>
#include <atomic>
#include <jsoncpp/json/json.h>


namespace ai_chat_server {

    struct ChatServerConfig {
        // server配置
        std::string _host = "0.0.0.0";
        uint16_t _port = 8080;
        spdlog::level::level_enum log_level = spdlog::level::info;  // 日志级别
        std::string _log_path = "stdout";  // 日志路径

        // sdk配置
        std::string _db_name = "chat_sdk.db";
        ai_chat_sdk::ModelConfig _model_config;
        ai_chat_sdk::ProviderConfigs _provider_configs;
        ai_chat_sdk::Models _models;
    };

    class ChatServer {
    private:
        httplib::Server _server;
        ai_chat_sdk::ChatSDK _chat_sdk;
        ChatServerConfig _config;
        std::atomic<bool> _is_running = {false};

    private:
        // 设置路由
        void SetHttpRoute();
        // 构建错误响应
        void BuildERRResponse(httplib::Response &res, int status, const std::string &message);
        // 创建新会话
        void CreateNewSession(const httplib::Request& req, httplib::Response& res);
        // 删除会话
        void DeleteSession(const httplib::Request& req, httplib::Response& res);
        // 获取会话列表
        void GetSessionList(const httplib::Request& req, httplib::Response& res);
        // 获取指定会话历史消息
        void GetSessionHistory(const httplib::Request& req, httplib::Response& res);
        // 获取可用模型
        void GetModels(const httplib::Request& req, httplib::Response& res);
        // 更改当前会话的模型参数
        void ChangeModelConfig(const httplib::Request& req, httplib::Response& res);
        // 更改当前会话的模型
        void ChangeModel(const httplib::Request& req, httplib::Response& res);
        // 全量返回消息
        void SendMessage(const httplib::Request& req, httplib::Response& res);
        // 流式返回消息
        void SendMessageStream(const httplib::Request& req, httplib::Response& res);

    public:
        ChatServer(ChatServerConfig config);
        bool Start();
        bool Stop();
    };

} // namespace ai_chat_server