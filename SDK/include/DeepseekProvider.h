#pragma once

#include "LLMProvider.h"

namespace ai_chat_sdk {
    // DeepseekProvider 是一个基于 Deepseek 的 LLM 提供者
    class DeepseekProvider {
    protected:
        bool _is_available = false;
        std::string _base_url;
        std::string _api_key;

    public:
        virtual bool InitProvider(std::map<std::string, std::string> &model_config);
        virtual bool IsAvailable();
        virtual std::string GetModels();
        virtual std::string GetDesc();
        virtual std::string SendMssage(std::vector<Message> messages, std::map<std::string, std::string> request_params);
        virtual std::string SendMessageStream(std::vector<Message> messages, std::map<std::string, std::string> request_params,
                                        std::function<void(std::string &message, bool flag)> callback);
                                        // 处理流式信息的回调函数 第一个参数表示消息内容，第二个参数表示是否是最后一条消息
    };
}