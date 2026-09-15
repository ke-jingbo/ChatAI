#include <gtest/gtest.h>
#include "../SDK/include/DeepseekProvider.h"
#include "../SDK/include/util/mylog.h"
#include <unistd.h>

TEST(DeepseekProvider, TestSendMssage) {
    std::map<std::string, std::string> request_params;
    DBG("deepseek_apikey: {}", std::getenv("deepseek_apikey"));
    request_params["api_key"] = std::getenv("deepseek_apikey");
    request_params["base_url"] = "https://api.deepseek.com";

    ai_chat_sdk::DeepseekProvider provider;
    provider.InitProvider(request_params);
    ASSERT_TRUE(provider.IsAvailable());

    ai_chat_sdk::Message message("user", "你好");
    std::vector<ai_chat_sdk::Message> messages;
    messages.push_back(message);
    auto res = provider.SendMssage(messages, request_params);
    INFO("res: {}", res);
}

int main(int argc, char **argv) {
    mylog::Logger::Init("testLLM", "stdout", spdlog::level::info);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}