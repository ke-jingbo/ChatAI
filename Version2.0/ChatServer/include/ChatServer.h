#pragma once
#include <ai_chat_sdk/ChatSDK.h>
#include <httplib.h>
#include <ai_chat_sdk/util/mylog.h>
#include <atomic>
#include <jsoncpp/json/json.h>
#include <sodium.h>
#include "EmailVerification.h"


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
        EmailVerificationService _email_verification_service;

    private:
        // 设置路由
        void SetHttpRoute();
        // 构建错误响应
        void BuildERRResponse(httplib::Response &res, int status, const std::string &message);
        // 获取Cookie中的值
        std::string GetCookieValue(const httplib::Request &req, const std::string &key);
        // 获取用户id
        std::string GetUserId(const httplib::Request &req);
        // 创建cookie id
        std::string CreateCookieId();


        // 用户相关操作
        // 邮箱验证码申请
        void SendEmailVerificationCode(const httplib::Request& req, httplib::Response& res);
        // 邮箱验证码验证
        void VerifyEmail(const httplib::Request& req, httplib::Response& res);
        // 注册用户
        void RegisterUser(const httplib::Request& req, httplib::Response& res);
        // 登录用户
        void LoginUser(const httplib::Request& req, httplib::Response& res);
        // 登出用户
        void LogoutUser(const httplib::Request& req, httplib::Response& res);
        // 获取用户信息
        void GetUserInfo(const httplib::Request& req, httplib::Response& res);
        // 更改用户头像
        void ChangeUserAvatar(const httplib::Request& req, httplib::Response& res);
        // 更改用户名称
        void ChangeUserName(const httplib::Request& req, httplib::Response& res);
        // 更改用户邮箱
        void ChangeUserEmail(const httplib::Request& req, httplib::Response& res);
        // 更改用户密码
        void ChangeUserPassword(const httplib::Request& req, httplib::Response& res);
        // 忘记密码
        void ForgetUserPassword(const httplib::Request& req, httplib::Response& res);

        // 会话相关操作
        // 创建新会话
        void CreateNewSession(const httplib::Request& req, httplib::Response& res);
        // 删除会话
        void DeleteSession(const httplib::Request& req, httplib::Response& res);
        // 删除所有会话
        void DeleteAllSession(const httplib::Request& req, httplib::Response& res);
        // 获取会话列表
        void GetSessionList(const httplib::Request& req, httplib::Response& res);
        // 获取指定会话历史消息
        void GetSessionHistory(const httplib::Request& req, httplib::Response& res);
        // 获取可用模型
        void GetModels(const httplib::Request& req, httplib::Response& res);
        // 更改当前会话的名称
        void ChangeSessionName(const httplib::Request& req, httplib::Response& res);
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
