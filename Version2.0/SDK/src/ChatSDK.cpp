#include "../include/ChatSDK.h"
#include "../include/DeepseekProvider.h"
#include "../include/MimoProvider.h"
#include "../include/OllamaLLMProvider.h"
#include "../include/KimiProvider.h"
#include "../include/util/mylog.h"


namespace ai_chat_sdk {
    ChatSDK::ChatSDK(std::string db_name) :_user_manager(db_name), _llm_manager() {}

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

    // 获取指定模型信息
    Model ChatSDK::GetModel(const std::string &model_name) {
        return _llm_manager.GetModel(model_name);
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


    // 用户相关操作
    // 创建用户
    std::string ChatSDK::CreateUser(const std::string user_name, 
                                    const std::string user_avatar_path, 
                                    const std::string email, 
                                    const std::string password, 
                                    const std::string cookie_id) {
        return _user_manager.CreateUser(user_name, user_avatar_path, email, password, cookie_id);
    }
    // 删除用户
    void ChatSDK::DeleteUser(const std::string user_id) {
        _user_manager.DeleteUser(user_id);
    }
    // 登出用户
    bool ChatSDK::LogoutUser(const std::string user_id) {
        return _user_manager.LogoutUser(user_id);
    }
    // 获取用户
    std::string ChatSDK::GetUserId(const std::string cookie_id) {
        return _user_manager.GetUserId(cookie_id);
    }
    std::shared_ptr<User> ChatSDK::GetUser(const std::string user_id) {
        return _user_manager.GetUser(user_id);
    }
    std::shared_ptr<User> ChatSDK::LoginUser(const std::string email, const std::string password) {
        return _user_manager.LoginUser(email, password);
    }
    // 更新用户头像
    bool ChatSDK::UpdateUserAvatar(const std::string user_id, const std::string avatar_path, std::shared_ptr<std::string> img) {
        return _user_manager.UpdateUserAvatar(user_id, avatar_path, img);
    }
    // 更新用户名称
    bool ChatSDK::UpdateUserName(const std::string user_id, const std::string user_name) {
        return _user_manager.UpdateUserName(user_id, user_name);
    }
    // 更新用户邮箱
    bool ChatSDK::UpdateUserEmail(const std::string user_id, const std::string email) {
        return _user_manager.UpdateUserEmail(user_id, email);
    }
    // 更新用户密码
    bool ChatSDK::UpdateUserPassword(const std::string user_id, const std::string password) {
        return _user_manager.UpdateUserPassword(user_id, password);
    }
    // 忘记密码
    bool ChatSDK::ForgetUserPassword(const std::string email, const std::string password) {
        return _user_manager.ForgetUserPassword(email, password);
    }
    // 获取用户个数
    int64_t ChatSDK::GetOnlineUserCount() {
        return _user_manager.GetOnlineUserCount();
    }


    // 会话相关操作
    // 创建会话
    std::string ChatSDK::CreateSession(const std::string user_id, const std::string model_name, const std::string session_name) {
        return _user_manager.CreateSession(user_id, model_name, session_name);
    }
    // 删除会话
    void ChatSDK::DeleteSession(const std::string user_id, const std::string session_id) {
        _user_manager.DeleteSession(user_id, session_id);
    }
    // 更新会话名称
    bool ChatSDK::UpdateSessionName(const std::string user_id, const std::string session_id, const std::string session_name) {
        return _user_manager.UpdateSessionName(user_id, session_id, session_name);
    }
    // 更新会话模型
    bool ChatSDK::UpdateSessionModel(const std::string user_id, const std::string session_id, const std::string model_name) {
        if(_llm_manager.IsModelAvailable(model_name) == false) {
            ERR("ChatSDK::UpdateSessionModel() model is not available: {}", model_name);
            return false;
        }
        return _user_manager.UpdateSession(user_id, session_id, model_name);
    }
    // 更新会话模型的参数
    bool ChatSDK::UpdateSessionModelConfig(const std::string user_id, const std::string session_id, Params &params) {
        std::string model_name = (_user_manager.GetSession(user_id, session_id))->_model_name;
        return _llm_manager.UpdateModelConfig(model_name, params);
    }
    // 获取所有会话列表
    std::vector<std::string> ChatSDK::GetSessions(const std::string user_id) {
        return _user_manager.GetSessions(user_id);
    }
    // 获取指定会话
    std::shared_ptr<Session> ChatSDK::GetSession(const std::string user_id, const std::string session_id) {
        return _user_manager.GetSession(user_id, session_id);
    }
    // 清空指定用户所有会话
    void ChatSDK::ClearAllSessions(const std::string user_id) {
        _user_manager.ClearAllSessions(user_id);
    }

    // 发送消息
    std::string ChatSDK::SendMessage(const std::string user_id, const std::string session_id, const std::string message) {
        // 先检查session是否存在
        if(_user_manager.GetSession(user_id, session_id) == nullptr) {
            ERR("ChatSDK::SendMessage() session is not found: {}:{}", user_id, session_id);
            return "";
        }
        // 获取模型和消息
        std::string modle_name = (_user_manager.GetSession(user_id, session_id))->_model_name;
        std::vector<Message> messages;
        Model model = _llm_manager.GetModel(modle_name);
        messages = _user_manager.GetSessionMessages(user_id, session_id);
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
        _user_manager.UpdateSessionMessages(user_id, session_id, req_message);
        _user_manager.UpdateSessionMessages(user_id, session_id, res_message);
        return res;
    }
    // 发送消息流式
    std::string ChatSDK::SendMessageStream(const std::string user_id, const std::string session_id, const std::string message, StreamCallback callback) {
        // 先检查session是否存在
        if(_user_manager.GetSession(user_id, session_id) == nullptr) {
            ERR("ChatSDK::SendMessageStream() session is not found: {}:{}", user_id, session_id);
            return "";
        }
        // 获取模型和消息
        std::string modle_name = (_user_manager.GetSession(user_id, session_id))->_model_name;
        std::vector<Message> messages;
        Model model = _llm_manager.GetModel(modle_name);
        messages = _user_manager.GetSessionMessages(user_id, session_id);
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
        _user_manager.UpdateSessionMessages(user_id, session_id, req_message);        
        _user_manager.UpdateSessionMessages(user_id, session_id, res_message);
        return res;
    }

}  // end namespace ai_chat_sdk
