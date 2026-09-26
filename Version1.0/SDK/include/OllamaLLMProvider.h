#pragma once

#include "LLMProvider.h"

namespace ai_chat_sdk {
    // 基类Provider
    class OllamaLLMProvider : public LLMProvider {
    protected:
        std::string _model_name;  // 本地部署的模型名称
        std::string _model_desc;  // 本地部署的模型描述

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