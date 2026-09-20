#include "../include/ChatSDK.h"
#include "../include/util/mylog.h"
#include "../include/DeepseekProvider.h"
#include "../include/MimoProvider.h"
#include "../include/OllamaLLMProvider.h"
#include "../include/KimiProvider.h"


namespace ai_chat_sdk {
    ChatSDK::ChatSDK(std::string db_name) :_session_manager(db_name), _llm_manager() {}

    // 注册所支持的LLM提供者/模型
    bool ChatSDK::RegisterAndInitLLMProvider(ProviderConfigs configs) {
        for(auto &config : configs) {
            std::map<std::string, std::string> provider_config;
            if(config->_provider == "DeepseekProvider") {
                // 注册提供者
                auto api_config = std::dynamic_pointer_cast<APIConfig>(config);
                std::unique_ptr<LLMProvider> provider = std::make_unique<DeepseekProvider>();
                if(!_llm_manager.RegisterProvider(move(provider))) {
                    ERR("ChatSDK::RegisterLLMProvider() register {} failed", api_config->_provider);
                    return false;
                }
                // 初始化提供者
                provider_config["api_key"] = api_config->_api_key;
                provider_config["base_url"] = "https://api.deepseek.com";
                if(!_llm_manager.InitProvider(api_config->_provider, provider_config)) {
                    ERR("ChatSDK::RegisterLLMProvider() init {} failed", api_config->_provider);
                    return false;
                }
            }
            else if(config->_provider == "MimoProvider") {
                auto api_config = std::dynamic_pointer_cast<APIConfig>(config);
                std::unique_ptr<LLMProvider> provider = std::make_unique<MimoProvider>();
                if(!_llm_manager.RegisterProvider(move(provider))) {
                    ERR("ChatSDK::RegisterLLMProvider() register {} failed", api_config->_provider);
                    return false;
                }
                provider_config["api_key"] = api_config->_api_key;
                provider_config["base_url"] = "https://api.xiaomimimo.com";
                if(!_llm_manager.InitProvider(api_config->_provider, provider_config)) {
                    ERR("ChatSDK::RegisterLLMProvider() init {} failed", api_config->_provider);
                    return false;
                }
            }
            else if(config->_provider == "KimiProvider") {
                auto api_config = std::dynamic_pointer_cast<APIConfig>(config);
                std::unique_ptr<LLMProvider> provider = std::make_unique<KimiProvider>();
                if(!_llm_manager.RegisterProvider(move(provider))) {
                    ERR("ChatSDK::RegisterLLMProvider() register {} failed", api_config->_provider);
                    return false;
                }
                provider_config["api_key"] = api_config->_api_key;
                provider_config["base_url"] = "https://api.moonshot.cn";
                if(!_llm_manager.InitProvider(api_config->_provider, provider_config)) {
                    ERR("ChatSDK::RegisterLLMProvider() init {} failed", api_config->_provider);
                    return false;
                }
            } 
            else if(config->_provider == "OllamaLLMProvider") {
                std::unique_ptr<LLMProvider> provider = std::make_unique<OllamaLLMProvider>();
                auto local_config = std::dynamic_pointer_cast<LocalConfig>(config);
                if(!_llm_manager.RegisterProvider(move(provider))) {
                    ERR("ChatSDK::RegisterLLMProvider() register {} failed", config->_provider);
                    return false;
                }
                provider_config["model_name"] = local_config->_name;
                provider_config["model_desc"] = local_config->_desc;
                provider_config["base_url"] = local_config->_path;
                if(!_llm_manager.InitProvider(local_config->_provider, provider_config)) {
                    ERR("ChatSDK::RegisterLLMProvider() init {} failed", config->_provider);
                    return false;
                }
            }
            else {
                ERR("ChatSDK::RegisterLLMProvider() provider not supported: {}", config->_provider);
                return false;
            }
        }
        return true;
    }

    // 注册所支持的模型
    bool ChatSDK::RegisterAndInitModel(Models models) {
        for(auto &model : models) {
            if(model._provider == "OllamaLLMProvider") {
                if(!_llm_manager.RegisterModel(model)) return false;
                if(!_llm_manager.InitModel(model._name)) return false;
            }
            else if(model._provider == "DeepseekProvider") {
                if(!_llm_manager.RegisterModel(model)) return false;
                if(!_llm_manager.InitModel(model._name)) return false;
            }
            else if(model._provider == "MimoProvider") {
                if(!_llm_manager.RegisterModel(model)) return false;
                if(!_llm_manager.InitModel(model._name)) return false;
            }
            else if(model._provider == "KimiProvider") {
                if(!_llm_manager.RegisterModel(model)) return false;
                if(!_llm_manager.InitModel(model._name)) return false;
            }
            else {
                ERR("ChatSDK::RegisterModel() model provider not found: {}", model._provider);
                return false;
            }
        }
        return true;
    }

    // 初始化LLMManager
    bool ChatSDK::InitLLMManager(ProviderConfigs provider_configs, Models models) {
        // 注册并初始化提供者/模型
        if(!RegisterAndInitLLMProvider(provider_configs)) return false;
        if(!RegisterAndInitModel(models)) return false;
        return true;
    }
    // 获取可用模型信息
    std::vector<Model> ChatSDK::GetAvailableModels() {
        return _llm_manager.GetInitModels();
    }

    // 创建会话
    std::string ChatSDK::CreateSession(const std::string model_name) {
        return _session_manager.CreateSession(model_name);
    }
    // 删除会话
    void ChatSDK::DeleteSession(const std::string session_id) {
        _session_manager.DeleteSession(session_id);
    }
    // 更新会话模型
    bool ChatSDK::UpdateSessionModel(const std::string session_id, const std::string model_name) {
        if(_llm_manager.IsModelAvailable(model_name) == false) {
            ERR("ChatSDK::UpdateSessionModel() model is not available: {}", model_name);
            return false;
        }
        return _session_manager.UpdateSession(session_id, model_name);
    }
    // 获取所有会话列表
    std::vector<std::string> ChatSDK::GetSessions() {
        return _session_manager.GetSessions();
    }
    // 获取指定会话
    std::shared_ptr<Session> ChatSDK::GetSession(const std::string session_id) {
        return _session_manager.GetSession(session_id);
    }
    // 清空所有会话
    void ChatSDK::ClearAllSessions() {
        _session_manager.ClearAllSessions();
    }

    // 发送消息
    std::string ChatSDK::SendMessage(const std::string session_id, const std::string message) {
        // 获取模型和消息
        std::string modle_name = (_session_manager.GetSession(session_id))->_model_name;
        std::vector<Message> messages;
        Model model = _llm_manager.GetModel(modle_name);
        messages = _session_manager.GetSessionMessages(session_id);
        Message req_message("user", message);
        messages.push_back(req_message);
        // 初始化请求参数
        Params request_params;
        request_params["temperature"] = std::to_string(model._config._temperature);
        request_params["max_tokens"] = std::to_string(model._config._max_tokens);
        request_params["think"] = model._config._think ? "true" : "false";
        request_params["reasoning_effort"] = model._config._reasoning_effort;
        // 发送消息
        std::string res = _llm_manager.SendMessage(model, messages, request_params);
        if(res.empty()) {
            ERR("ChatSDK::SendMessage() send message failed");
            return "";
        }
        // 更新会话消息
        Message res_message("assistant", res);
        _session_manager.UpdateSessionMessages(session_id, req_message);
        _session_manager.UpdateSessionMessages(session_id, res_message);
        return res;
    }
    // 发送消息流式
    std::string ChatSDK::SendMessageStream(const std::string session_id, const std::string message, StreamCallback callback) {
        // 获取模型和消息
        std::string modle_name = (_session_manager.GetSession(session_id))->_model_name;
        std::vector<Message> messages;
        Model model = _llm_manager.GetModel(modle_name);
        messages = _session_manager.GetSessionMessages(session_id);
        Message req_message("user", message);
        messages.push_back(req_message);
        // 初始化请求参数
        Params request_params;
        request_params["temperature"] = std::to_string(model._config._temperature);
        request_params["max_tokens"] = std::to_string(model._config._max_tokens);
        request_params["think"] = model._config._think ? "true" : "false";
        request_params["reasoning_effort"] = model._config._reasoning_effort;
        // 发送消息流式
        std::string res = _llm_manager.SendMessageStream(model, messages, request_params, callback);
        if(res.empty()) {
            ERR("ChatSDK::SendMessageStream() send message failed");
            return "";
        }
        // 更新会话消息
        Message res_message("assistant", res);
        _session_manager.UpdateSessionMessages(session_id, req_message);        
        _session_manager.UpdateSessionMessages(session_id, res_message);
        return res;
    }

}  // end namespace ai_chat_sdk
