#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include "common.h"

namespace ai_chat_sdk {
    // 基类Provider
    class LLMProvider {
    protected:
        bool _is_available = false;
        std::string _base_url;
        std::string _api_key;

    public:
        virtual bool InitProvider(Params &model_config) = 0;
        virtual bool IsAvailable() = 0;
        virtual std::string GetModels() = 0;
        virtual std::string GetDesc() = 0;
        virtual std::string SendMessage(Messages messages, Params request_params) = 0;
        virtual std::string SendMessageStream(Messages messages, Params request_params, StreamCallback callback) = 0;
    };
}