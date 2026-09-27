#pragma once

#include "Common.h"
#include "LLMManager.h"
#include "SessionManager.h"
#include "UserManager.h"


namespace ai_chat_sdk {
    using ProviderConfigs = std::vector<std::shared_ptr<ProviderConfig>>;
    using Models = std::vector<Model>;
    class ChatSDK {
    private:
        LLMManager _llm_manager;
        UserManager _user_manager;
        bool _is_init = false;

    private:
        // 注册并初始化所支持的LLM提供者/模型
        bool RegisterAndInitLLMProvider(std::vector<std::shared_ptr<ProviderConfig>> configs);
        bool RegisterAndInitModel(std::vector<Model> models);

    public:
        ChatSDK(std::string db_name = "chat_sdk.db");
        // LLM相关操作
        // 初始化LLMManager
        bool InitLLMManager(std::vector<std::shared_ptr<ProviderConfig>> provider_configs, std::vector<Model> models);
        // 获取可用模型信息
        std::vector<Model> GetAvailableModels();
        // 获取指定模型信息
        Model GetModel(const std::string &model_name);

        // 用户相关操作
        // 创建用户
        std::string CreateUser(const std::string user_name, 
                            const std::string user_avatar_path, 
                            const std::string email, 
                            const std::string password,
                            const std::string cookie_id);
        // 删除用户
        void DeleteUser(const std::string user_id);
        // 登出用户
        bool LogoutUser(const std::string user_id);
        // 获取用户
        std::string GetUserId(const std::string cookie_id);
        std::shared_ptr<User> GetUser(const std::string user_id);
        std::shared_ptr<User> LoginUser(const std::string email, const std::string password);
        // 更新用户头像
        bool UpdateUserAvatar(const std::string user_id, const std::string avatar_path, std::shared_ptr<std::string> img);
        // 更新用户名称
        bool UpdateUserName(const std::string user_id, const std::string user_name);
        // 更新用户邮箱
        bool UpdateUserEmail(const std::string user_id, const std::string email);
        // 更新用户密码
        bool UpdateUserPassword(const std::string user_id, const std::string password);
        // 忘记密码
        bool ForgetUserPassword(const std::string email, const std::string password);
        // 获取用户个数
        int64_t GetOnlineUserCount();

        // 会话相关操作
        // 创建会话
        std::string CreateSession(const std::string user_id, const std::string model_name, const std::string session_name = "new session");
        // 删除会话
        void DeleteSession(const std:: string user_id, const std::string session_id);
        // 更新会话名称
        bool UpdateSessionName(const std::string user_id, const std::string session_id, const std::string session_name);
        // 更新会话模型
        bool UpdateSessionModel(const std::string user_id, const std::string session_id, const std::string model_name);
        // 更新会话模型的参数
        bool UpdateSessionModelConfig(const std::string user_id, const std::string session_id, Params &params);
        // 获取所有会话列表
        std::vector<std::string> GetSessions(const std::string user_id);
        // 获取指定会话
        std::shared_ptr<Session> GetSession(const std::string user_id, const std::string session_id);
        // 清空所有会话
        void ClearAllSessions(const std::string user_id);

        // 发送消息
        std::string SendMessage(const std::string user_id, const std::string session_id, const std::string message);
        // 发送消息流式
        std::string SendMessageStream(const std::string user_id, const std::string session_id, const std::string message, StreamCallback callback);
    };
}