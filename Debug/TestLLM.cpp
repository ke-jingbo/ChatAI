#include <gtest/gtest.h>
#include "../SDK/include/DeepseekProvider.h"
#include "../SDK/include/MimoProvider.h"
#include "../SDK/include/util/mylog.h"
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

TEST(MimoProvider, TestSendMssage) {
    std::map<std::string, std::string> request_params;
    request_params["api_key"] = std::getenv("mimo_apikey");
    request_params["base_url"] = "https://api.xiaomimimo.com/v1";

    ai_chat_sdk::MimoProvider provider;
    provider.InitProvider(request_params);
    ASSERT_TRUE(provider.IsAvailable());

    ai_chat_sdk::Message message("user", "你好,请介绍一下你自己");
    std::vector<ai_chat_sdk::Message> messages;
    messages.push_back(message);
    // auto func = [](const std::string &message, bool flag) {
    //     // if(message.empty()) return;
    //     // std::cout << message;
    //     INFO("{}", message);
    // };
    auto res = provider.SendMessage(messages, request_params);
    INFO("res: {}", res);
}

int main(int argc, char **argv) {
    mylog::Logger::Init("testLLM", "stdout", spdlog::level::info);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}