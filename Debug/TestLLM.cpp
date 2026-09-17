#include <gtest/gtest.h>
#include "../SDK/include/DeepseekProvider.h"
#include "../SDK/include/MimoProvider.h"
#include "../SDK/include/KimiProvider.h"
#include "../SDK/include/util/mylog.h"
#include "../SDK/include/OllamaLLMProvider.h"
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

TEST(OllamaLLMProvider, TestSendMssage) {
    std::map<std::string, std::string> model_params;
    model_params["model_name"] = "qwen3:0.6b";
    model_params["model_desc"] = "这是一个测试模型";
    model_params["base_url"] = "http://127.0.0.1:11434";

    std::unique_ptr<ai_chat_sdk::LLMProvider> provider(new ai_chat_sdk::OllamaLLMProvider());
    provider->InitProvider(model_params);
    ASSERT_TRUE(provider->IsAvailable());
    std::string content;
    std::cout << "Enter# ";
    std::cin >> content;
    ai_chat_sdk::Message message("user", content);
    std::vector<ai_chat_sdk::Message> messages;
    messages.push_back(message);
    auto func = [](const std::string &message, bool flag) {
        if(message.empty()) return;
        std::cout << message << std::flush;
        // INFO("{}", message);
    };
    std::map<std::string, std::string> request_params;
    request_params["think"] = "true";
    request_params["temperature"] = "0.8";
    auto res = provider->SendMessageStream(messages, request_params, func);
    ASSERT_FALSE(res.empty());
    // INFO("res: {}", res);
    std::cout << std::endl;
}


int main(int argc, char **argv) {
    mylog::Logger::Init("testLLM", "stdout", spdlog::level::info);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}