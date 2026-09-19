#pragma once

#include "Common.h"
#include "LLMProvider.h"
#include <map>
#include <memory>

namespace ai_chat_sdk {
    class LLMManager {
    private:
        std::map<std::string, std::unique_ptr<LLMProvider>> _providers;  // 提供者列表
        std::map<std::string, Model> _models;                            // 模型列表
    public:
        // 注册提供者/模型
        bool RegisterProvider(std::unique_ptr<LLMProvider> &provider);
        bool RegisterModel(Model model_name);
        // 初始化提供者
        bool InitProvider(const std::string &provider_name, Params &model_config);
        bool InitModel(const std::string &model_name);
        // 查看提供者/模型是否可用
        bool IsProviderAvailable(const std::string &provider_name);
        bool IsModelAvailable(const std::string &model_name);
        // 获取可用模型列表
        std::vector<Model> GetInitModels();
        // 发送消息
        std::string SendMessage(Model model_name, Messages messages, Params request_params);
        // 流式发送消息
        std::string SendMessageStream(Model model_name, Messages messages, Params request_params, StreamCallback callback);
    };
}