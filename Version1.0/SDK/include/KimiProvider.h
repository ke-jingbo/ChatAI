#pragma once

#include "LLMProvider.h"

namespace ai_chat_sdk {
    // KimiProvider 是一个基于 Kimi 的 LLM 提供者
    class KimiProvider : public LLMProvider {
    protected:
        bool _is_available = false;
        std::string _base_url;
        std::string _api_key;

    public:
        virtual bool InitProvider(Params &provider_config) override;
        virtual bool IsAvailable() override;
        virtual std::string GetProviderName() override;
        virtual std::string GetDesc() override;
        virtual std::string BuildRequestBody(Model model, Messages messages, Params request_params, bool isstream) override;
        virtual std::string SendMessage(Model model, Messages messages, Params request_params) override;
        virtual std::string SendMessageStream(Model model, Messages messages, Params request_params, StreamCallback callback) override;
    };
}