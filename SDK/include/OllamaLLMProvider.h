#pragma once

#include "LLMProvider.h"

namespace ai_chat_sdk {
    // 基类Provider
    class OllamaLLMProvider : public LLMProvider {
    protected:
        std::string _model_name;  // 本地部署的模型名称
        std::string _model_desc;  // 本地部署的模型描述

    public:
        virtual bool InitProvider(Params &model_config) override;
        virtual bool IsAvailable() override;
        virtual std::string GetModels() override;
        virtual std::string GetDesc() override;
        virtual std::string BuildRequestBody(Messages messages, Params request_params, bool isstream) override;
        virtual std::string SendMessage(Messages messages, Params request_params) override;
        virtual std::string SendMessageStream(Messages messages, Params request_params, StreamCallback callback) override;
    };
}