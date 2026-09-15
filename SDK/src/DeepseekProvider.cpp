#include "../include/DeepseekProvider.h"
#include "../include/util/mylog.h"
#include <jsoncpp/json/json.h>
#include <httplib.h>

namespace ai_chat_sdk {
    bool DeepseekProvider::InitProvider(std::map<std::string, std::string> &request_params) {
        // 查找api_key
        auto api_key = request_params.find("api_key");
        if(api_key != request_params.end()) {
            _api_key = api_key->second;
        }
        else{
            ERR("DeepseekProvider::InitProvider() api_key is not found");
            return false;
        }
        // 查找base_url
        auto base_url = request_params.find("base_url");
        if(base_url != request_params.end()) {
            _base_url = base_url->second;
        }
        else{
            ERR("DeepseekProvider::InitProvider() base_url is not found");
            return false;
        }
        INFO("api_key: {}, base_url: {}", _api_key.c_str(), _base_url.c_str()); 
        _is_available = true;
        return true;
    }

    bool DeepseekProvider::IsAvailable() {return _is_available;}
    std::string DeepseekProvider::GetModels() {return "deepseek-flash";}
    std::string DeepseekProvider::GetDesc() {
        return "DeepSeek 的轻量版快速版模型。响应速度更快、推理成本更低，适合处理日常、高频的对话任务";
    }

    std::string DeepseekProvider::SendMssage(std::vector<Message> messages, std::map<std::string, std::string> request_params) {
        // 1. 先检测模型是否可用
        if(!_is_available) {
            ERR("DeepseekProvider::SendMssage() provider is not available");
            return "";
        }
        // 2. 构造请求参数
        // 如果有温度和最大token数，则使用指定的参数 否则使用默认值
        double temperature = 0.8;
        int max_tokens = 2048;
        if(request_params.find("temperature") != request_params.end()) {
            temperature = std::stod(request_params["temperature"]);
        }
        if(request_params.find("max_tokens") != request_params.end()) {
            max_tokens = std::stoi(request_params["max_tokens"]);
        }
        // 3. 构建历史消息
        Json::Value message_array(Json::arrayValue);
        for(auto &message : messages) {
            Json::Value message_obj;
            message_obj["role"] = message._role;
            message_obj["content"] = message._content;
            message_array.append(message_obj);
        }
        // 4. 构建请求参数
        Json::Value request_obj;
        request_obj["model"] = "deepseek-flash";
        request_obj["messages"] = message_array;
        request_obj["temperature"] = temperature;
        request_obj["max_tokens"] = max_tokens;
        request_obj["stream"] = false;
        // 5. 序列化请求参数
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        std::string request_str;
        request_str = Json::writeString(builder, request_obj);
        INFO("request_str: {}", request_str.c_str());
        // 6. 构建client 发送POST请求
        httplib::Client client(_base_url);
        client.set_connection_timeout(10, 0);  // 设置超时时间为10秒
        client.set_read_timeout(60, 0);        // 设置读取超时时间为60秒
        // 构建请求头
        httplib::Headers headers = {
            {"Content-Type", "application/json"},
            {"Authorization", "Bearer " + _api_key}
        };
        // 发送POST请求
        auto res = client.Post("/chat/completions", headers, request_str, "application/json");
        if(!res) {
            ERR("DeepseekProvider::SendMssage() request failed");
            return "";
        }
        INFO("response status: {}", res->status);
        INFO("response_str: {}", res->body);

        if(res->status != 200) {
            ERR("DeepseekProvider::SendMssage() request failed");
            return "";
        }

        // 7. 如果是正常的响应，则解析响应内容
        // 先将res的body转换为json对象
        Json::Value root;
        Json::CharReaderBuilder read_builder;
        std::string errs;
        std::stringstream ss(res->body);
        bool parsingSuccessful = Json::parseFromStream(read_builder, ss, &root, &errs);
        if(!parsingSuccessful) {
            ERR("DeepseekProvider::SendMssage() parse json failed");
            return "";
        }
        // 检测是否有choices数组
        if(root.isMember("choices") && root["choices"].isArray() && !root["choices"].empty()) {
            // 如果有choices数组，则解析choices数组
            Json::Value choices = root["choices"];
            auto choice = choices[0];
            if(choice.isMember("message") && choice["message"].isMember("content")) {
                // 如果有message字段，则解析message字段
                std::string content = choice["message"]["content"].asString();
                return content;
            }
        }
        ERR("DeepseekProvider::SendMssage() parse json failed");
        return "";
    }

    std::string DeepseekProvider::SendMessageStream(std::vector<Message> messages, std::map<std::string, std::string> request_params,
                                        std::function<void(std::string &message, bool flag)> callback) {
        return "";
    }
}