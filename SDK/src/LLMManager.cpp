#include "../include/LLMManager.h"
#include "../include/util/mylog.h"

namespace ai_chat_sdk {
    // 注册提供者/模型
    bool LLMManager::RegisterProvider(std::unique_ptr<LLMProvider> provider) {
        assert(_providers.find(provider->GetProviderName()) == _providers.end());  // 检查提供者名称是否已注册
        if(!provider) {
            ERR("LLMManager::RegisterProvider() provider is nullptr");
            return false;
        }
        _providers[provider->GetProviderName()] = std::move(provider);
        return true;
    }
    bool LLMManager::RegisterModel(Model model) {
        assert(_models.find(model._name) == _models.end());  // 检查模型名称是否已注册
        if(model._name.empty()) {
            ERR("LLMManager::RegisterModel() model_name is empty");
            return false;
        }
        _models.emplace(model._name, model);
        return true;
    }

    // 初始化提供者/模型
    bool LLMManager::InitProvider(const std::string &provider_name, Params &model_config) {
        if(_providers.find(provider_name) == _providers.end()) {
            ERR("LLMManager::InitProvider() provider not found: {}", provider_name);
            return false;
        }
        return _providers[provider_name]->InitProvider(model_config);
    }
    bool LLMManager::InitModel(const std::string &model_name) {
        if(_models.find(model_name) == _models.end()) {
            ERR("LLMManager::InitModel() model not found: {}", model_name);
            return false;
        }
        if(_providers.find(_models[model_name]._provider) == _providers.end()) {
            ERR("LLMManager::InitModel() provider not found: {}", _models[model_name]._provider);
            return false;
        }
        if(_providers[_models[model_name]._provider]->IsAvailable()) {
            _models[model_name]._is_active = true;
            return true;
        }
        else{
            ERR("LLMManager::InitModel() provider is not available: {}", _models[model_name]._provider);
            return false;
        }
    }

    // 查看提供者/模型是否可用
    bool LLMManager::IsProviderAvailable(const std::string &provider_name){
        if(_providers.find(provider_name) == _providers.end()) {
            ERR("LLMManager::IsProviderAvailable() provider not found: {}", provider_name);
            return false;
        }
        return _providers[provider_name]->IsAvailable();
    }
    bool LLMManager::IsModelAvailable(const std::string &model_name){
        if(_models.find(model_name) == _models.end()) {
            ERR("LLMManager::IsModelAvailable() model not found: {}", model_name);
            return false;
        }
        return _models[model_name]._is_active;
    }

    // 获取可用模型列表
    std::vector<Model> LLMManager::GetInitModels() {
        std::vector<Model> models;
        for(auto &p : _models) {
            if(p.second._is_active) models.push_back(p.second);
        }
        return models;
    }
    // 获取指定可用模型
    Model LLMManager::GetModel(const std::string &model_name) {
        if(_models.find(model_name) == _models.end()) {
            ERR("LLMManager::GetModel() model not found: {}", model_name);
            return Model();
        }
        return _models[model_name];
    }

    // 发送消息
    std::string LLMManager::SendMessage(Model model, Messages messages, Params request_params) {
        if(_models.find(model._name) == _models.end()) {
            ERR("LLMManager::SendMessage() model not found: {}", model._name);
            return "";
        }
        if(IsModelAvailable(model._name) == false) {
            ERR("LLMManager::SendMessage() model is not available: {}", model._name);
            return "";
        }
        return _providers[_models[model._name]._provider]->SendMessage(model, messages, request_params);
    }

    // 流式发送消息
    std::string LLMManager::SendMessageStream(Model model, Messages messages, Params request_params, StreamCallback callback) {
        if(_models.find(model._name) == _models.end()) {
            ERR("LLMManager::SendMessageStream() model not found: {}", model._name);
            return "";
        }
        if(IsModelAvailable(model._name) == false) {
            ERR("LLMManager::SendMessageStream() model is not available: {}", model._name);
            return "";
        }
        return _providers[_models[model._name]._provider]->SendMessageStream(model, messages, request_params, callback);
    }
}