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
        virtual bool InitProvider(std::map<std::string, std::string> &model_config) = 0;
        virtual bool IsAvailable() = 0;
        virtual std::string GetModels() = 0;
        virtual std::string GetDesc() = 0;
        virtual std::string SendMssage(std::vector<Message> messages, std::map<std::string, std::string> request_params) = 0;
        virtual std::string SendMessageStream(std::vector<Message> messages, std::map<std::string, std::string> request_params,
                                        std::function<void(std::string &message, bool flag)> callback) = 0;
                                        // 处理流式信息的回调函数 第一个参数表示消息内容，第二个参数表示是否是最后一条消息
    };
}