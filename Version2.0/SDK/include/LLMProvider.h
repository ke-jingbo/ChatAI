#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include "Common.h"

namespace ai_chat_sdk {
    // 基类Provider
    class LLMProvider {
    protected:
        bool _is_available = false;
        std::string _base_url;
        std::string _api_key;

    public:
        virtual bool InitProvider(Params &provider_config) = 0;
        virtual bool IsAvailable() = 0;
        virtual std::string GetProviderName() = 0;
        virtual std::string GetDesc() = 0;
        virtual std::string BuildRequestBody(Model model, Messages messages, Params request_params, bool isstream) = 0;
        virtual std::string SendMessage(Model model, Messages messages, Params request_params) = 0;
        virtual std::string SendMessageStream(Model model, Messages messages, Params request_params, StreamCallback callback) = 0;
    };
}