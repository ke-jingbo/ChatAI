#include <gtest/gtest.h>
#include "../SDK/include/DeepseekProvider.h"
#include "../SDK/include/MimoProvider.h"
#include "../SDK/include/KimiProvider.h"
#include "../SDK/include/util/mylog.h"
#include "../SDK/include/OllamaLLMProvider.h"
#include "../SDK/include/LLMManager.h"
#include "../SDK/include/ChatSDK.h"
#include <unistd.h>

// TEST(DeepseekProvider, TestSendMssage) {
//     std::map<std::string, std::string> request_params;
//     request_params["api_key"] = std::getenv("deepseek_apikey");
//     request_params["base_url"] = "https://api.deepseek.com";

//     ai_chat_sdk::DeepseekProvider provider;
//     provider.InitProvider(request_params);
//     ASSERT_TRUE(provider.IsAvailable());

//     ai_chat_sdk::Message message("user", "你好");
//     std::vector<ai_chat_sdk::Message> messages;
//     messages.push_back(message);
//     auto func = [](const std::string &message, bool flag) {
//         // if(message.empty()) return;
//         // std::cout << message;
//         INFO("{}", message);
//     };
//     auto res = provider.SendMessageStream(messages, request_params, func);
//     INFO("res: {}", res);
// }

// TEST(MimoProvider, TestSendMssage) {
//     std::map<std::string, std::string> model_params;
//     model_params["api_key"] = std::getenv("mimo_apikey");
//     model_params["base_url"] = "https://api.xiaomimimo.com";

//     ai_chat_sdk::MimoProvider provider;
//     provider.InitProvider(model_params);
//     ASSERT_TRUE(provider.IsAvailable());

//     ai_chat_sdk::Message message("user", "你好,请介绍一下你自己");
//     std::vector<ai_chat_sdk::Message> messages;
//     messages.push_back(message);
//     auto func = [](const std::string &message, bool flag) {
//         if(message.empty()) return;
//         std::cout << message << std::flush;
//         // INFO("{}", message);
//     };
//     std::map<std::string, std::string> request_params;
//     request_params["temperature"] = "1.2";
//     request_params["max_tokens"] = "2048";
//     auto res = provider.SendMessageStream(messages, request_params, func);
//     ASSERT_FALSE(res.empty());
//     std::cout << std::endl;
//     // INFO("res: {}", res);
// }

// TEST(KimiProvider, TestSendMssage) {
//     std::map<std::string, std::string> model_params;
//     model_params["api_key"] = std::getenv("kimi_apikey");
//     model_params["base_url"] = "https://api.moonshot.cn";

//     std::unique_ptr<ai_chat_sdk::LLMProvider> provider(new ai_chat_sdk::KimiProvider());
//     provider->InitProvider(model_params);
//     ASSERT_TRUE(provider->IsAvailable());
//     std::string content;
//     std::cout << "Enter# ";
//     std::cin >> content;
//     ai_chat_sdk::Message message("user", content);
//     std::vector<ai_chat_sdk::Message> messages;
//     messages.push_back(message);
//     auto func = [](const std::string &message, bool flag) {
//         if(message.empty()) return;
//         std::cout << message << std::flush;
//         // INFO("{}", message);
//     };
//     std::map<std::string, std::string> request_params;
//     request_params["max_tokens"] = "2048";
//     request_params["reasoning_effort"] = "max";
//     auto res = provider->SendMessageStream(messages, request_params, func);
//     ASSERT_FALSE(res.empty());
//     // INFO("res: {}", res);
//     std::cout << std::endl;
// }

// TEST(OllamaLLMProvider, TestSendMssage) {
//     std::map<std::string, std::string> model_params;
//     model_params["model_name"] = "qwen3:0.6b";
//     model_params["model_desc"] = "这是一个测试模型";
//     model_params["base_url"] = "http://127.0.0.1:11434";

//     std::unique_ptr<ai_chat_sdk::LLMProvider> provider(new ai_chat_sdk::OllamaLLMProvider());
//     provider->InitProvider(model_params);
//     ASSERT_TRUE(provider->IsAvailable());
//     std::string content;
//     std::cout << "Enter# ";
//     std::cin >> content;
//     ai_chat_sdk::Message message("user", content);
//     std::vector<ai_chat_sdk::Message> messages;
//     messages.push_back(message);
//     auto func = [](const std::string &message, bool flag) {
//         if(message.empty()) return;
//         std::cout << message << std::flush;
//         // INFO("{}", message);
//     };
//     std::map<std::string, std::string> request_params;
//     request_params["think"] = "true";
//     request_params["temperature"] = "0.8";
//     auto res = provider->SendMessageStream(messages, request_params, func);
//     ASSERT_FALSE(res.empty());
//     // INFO("res: {}", res);
//     std::cout << std::endl;
// }

// TEST(LLMManager, TestRegisterProvider) {
//     std::unique_ptr<ai_chat_sdk::LLMProvider> provider(new ai_chat_sdk::OllamaLLMProvider());
//     ai_chat_sdk::LLMManager manager;
//     ASSERT_TRUE(manager.RegisterProvider(provider));
//     // 测试初始化提供者
//     ai_chat_sdk::Params provider_config;
//     provider_config["model_name"] = "qwen3:0.6b";
//     provider_config["model_desc"] = "这是一个测试模型";
//     provider_config["base_url"] = "http://127.0.0.1:11434";
//     manager.InitProvider("OllamaLLMProvider", provider_config);
//     ASSERT_TRUE(manager.IsProviderAvailable("OllamaLLMProvider"));
//     // 测试初始化模型
//     ai_chat_sdk::Model model("qwen3:0.6b", "http://127.0.0.1:11434", "OllamaLLMProvider");
//     manager.RegisterModel(model);
//     manager.InitModel("qwen3:0.6b");
//     std::string content;
//     std::cout << "Enter# ";
//     std::cin >> content;
//     ai_chat_sdk::Message message("user", content);
//     std::vector<ai_chat_sdk::Message> messages;
//     messages.push_back(message);
//     auto func = [](const std::string &message, bool flag) {
//         if(message.empty()) return;
//         std::cout << message << std::flush;
//         // INFO("{}", message);
//     };
//     std::map<std::string, std::string> request_params;
//     request_params["temperature"] = "1.2";
//     request_params["max_tokens"] = "2048";
//     auto res = manager.SendMessageStream(model, messages, request_params, func);
//     std::cout << std::endl;
//     ASSERT_FALSE(res.empty());
// }


TEST(ChatSDK, TestSendMssage) {
    std::unique_ptr<ai_chat_sdk::ChatSDK> sdk(new ai_chat_sdk::ChatSDK());
    // sdk->ClearAllSessions();
    ai_chat_sdk::ProviderConfigs provider_configs;
    std::shared_ptr<ai_chat_sdk::ProviderConfig> deepseek_provider_config(
        new ai_chat_sdk::APIConfig("DeepseekProvider", std::getenv("deepseek_apikey")));
    std::shared_ptr<ai_chat_sdk::ProviderConfig> mimo_provider_config(
        new ai_chat_sdk::APIConfig("MimoProvider", std::getenv("mimo_apikey")));
    std::shared_ptr<ai_chat_sdk::ProviderConfig> kimi_provider_config(
        new ai_chat_sdk::APIConfig("KimiProvider", std::getenv("kimi_apikey")));
    std::shared_ptr<ai_chat_sdk::ProviderConfig> ollama_provider_config(
        new ai_chat_sdk::LocalConfig("OllamaLLMProvider", "qwen3:0.6b", "http://127.0.0.1:11434"));
    provider_configs.push_back(deepseek_provider_config);
    provider_configs.push_back(mimo_provider_config);
    provider_configs.push_back(kimi_provider_config);
    provider_configs.push_back(ollama_provider_config);
    ai_chat_sdk::Models models;
    ai_chat_sdk::Model deepseek_model("deepseek-flash", "DeepseekProvider", "这是一个测试模型", ai_chat_sdk::ModelConfig());
    ai_chat_sdk::Model mimo_model("mimo-v2.5-pro", "MimoProvider", "这是一个测试模型", ai_chat_sdk::ModelConfig());
    ai_chat_sdk::Model kimi_model("kimi-k3", "KimiProvider", "这是一个测试模型", ai_chat_sdk::ModelConfig());
    ai_chat_sdk::Model kimi_model2("kimi-k2.6", "KimiProvider", "这是一个测试模型", ai_chat_sdk::ModelConfig());
    ai_chat_sdk::Model qwen3_model("qwen3:0.6b", "OllamaLLMProvider", "这是一个测试模型", ai_chat_sdk::ModelConfig());
    models.push_back(deepseek_model);
    models.push_back(mimo_model);
    models.push_back(kimi_model);
    models.push_back(kimi_model2);
    models.push_back(qwen3_model);
    ASSERT_TRUE(sdk->InitLLMManager(provider_configs, models));
    std::vector<std::string> session_ids = sdk->GetSessions();
    std::string session_id;
    if(session_ids.empty()) {
        std::cout << "没有会话，请先创建会话" << std::endl;
        session_id = sdk->CreateSession("deepseek-flash");
    }
    else {
        std::cout << "当前会话：" << session_ids[0] << std::endl;
        session_id = session_ids[0];
    }
    int op = 0;
    do {
        std::cout << "-----------------------请输入操作：----------------------" << std::endl;
        std::cout << "1. 发送消息" << std::endl;
        std::cout << "2. 发送消息流式" << std::endl;
        std::cout << "3. 更新会话模型" << std::endl;
        std::cout << "0. 退出" << std::endl;
        std::cout << "--------------------------------------------------------" << std::endl;
        std::cout << "Enter# ";
        std::cin >> op;
        switch(op) {
            case 1: {
                std::cout << "请输入消息内容：";
                std::string message;
                std::cin >> message;
                std::cout << sdk->SendMessage(session_id, message) << std::endl;
                break;
            }
            case 2: {
                std::cout << "请输入消息内容：";
                std::string message;
                std::cin >> message;
                sdk->SendMessageStream(session_id, message, [](const std::string &message, bool flag) {
                    std::cout << message << std::flush;
                });
                std::cout << std::endl;
                break;
            }
            case 3: {
                std::cout << "deepseek-flash" << std::endl;
                std::cout << "mimo-v2.5-pro" << std::endl;
                std::cout << "kimi-k3" << std::endl;
                std::cout << "kimi-k2.6" << std::endl;
                std::cout << "qwen3:0.6b" << std::endl;
                std::cout << "请输入模型名称：";
                std::string model_name;
                std::cin >> model_name;
                if(sdk->UpdateSessionModel(session_id, model_name)) 
                    std::cout << "更新成功：" << model_name << std::endl;
                else std::cout << "更新失败：" << model_name << std::endl;
                break;
            }
            case 0: {
                op = 0;
                break;
            }
            default:
                std::cout << "输入错误，请重新输入" << std::endl;
                break;
        }
    } while(op != 0);
}

int main(int argc, char **argv) {
    mylog::Logger::Init("testLLM", "stdout", spdlog::level::info);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}