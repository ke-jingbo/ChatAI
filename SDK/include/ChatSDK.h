#pragma once

#include "Common.h"
#include "LLMManager.h"
#include "SessionManager.h"

namespace ai_chat_sdk {
    using ProviderConfigs = std::vector<std::shared_ptr<ProviderConfig>>;
    using Models = std::vector<Model>;
    class ChatSDK {
    private:
        LLMManager _llm_manager;
        SessionManager _session_manager;
        bool _is_init = false;

    private:
        // 注册并初始化所支持的LLM提供者/模型
        bool RegisterAndInitLLMProvider(std::vector<std::shared_ptr<ProviderConfig>> configs);
        bool RegisterAndInitModel(std::vector<Model> models);

    public:
        ChatSDK(std::string db_name = "chat_sdk.db");
        // 初始化LLMManager
        bool InitLLMManager(std::vector<std::shared_ptr<ProviderConfig>> provider_configs, std::vector<Model> models);
        // 获取可用模型信息
        std::vector<Model> GetAvailableModels();

        // 创建会话
        std::string CreateSession(const std::string model_name);
        // 删除会话
        void DeleteSession(const std::string session_id);
        // 更新会话模型
        void UpdateSessionModel(const std::string session_id, const std::string model_name);
        // 获取所有会话列表
        std::vector<std::string> GetSessions();
        // 获取指定会话
        std::shared_ptr<Session> GetSession(const std::string session_id);

        // 发送消息
        std::string SendMessage(const std::string session_id, const std::string message);
        // 发送消息流式
        std::string SendMessageStream(const std::string session_id, const std::string message, StreamCallback callback);
    };
}