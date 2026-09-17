#pragma once

#include "LLMProvider.h"

namespace ai_chat_sdk {
    // DeepseekProvider 是一个基于 Deepseek 的 LLM 提供者
    class DeepseekProvider : public LLMProvider {
    protected:
        bool _is_available = false;
        std::string _base_url;
        std::string _api_key;

    public:
        virtual bool InitProvider(Params &model_config);
        virtual bool IsAvailable();
        virtual std::string GetModels();
        virtual std::string GetDesc();
        virtual std::string BuildRequestBody(Messages messages, Params request_params, bool isstream);
        virtual std::string SendMessage(Messages messages, Params request_params);
        virtual std::string SendMessageStream(Messages messages, Params request_params, StreamCallback callback);                           
    };
}