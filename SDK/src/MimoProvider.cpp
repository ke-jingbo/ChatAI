#include "../include/MimoProvider.h"
#include "../include/util/mylog.h"
#include <jsoncpp/json/json.h>
#include <httplib.h>

namespace ai_chat_sdk {
    // 初始化接口
    bool MimoProvider::InitProvider(Params &request_params) {
        // 查找api_key
        auto api_key = request_params.find("api_key");
        if(api_key != request_params.end()) {
            _api_key = api_key->second;
        }
        else{
            ERR("MimoProvider::InitProvider() api_key is not found");
            return false;
        }
        // 查找base_url
        auto base_url = request_params.find("base_url");
        if(base_url != request_params.end()) {
            _base_url = base_url->second;
        }
        else{
            ERR("MimoProvider::InitProvider() base_url is not found");
            return false;
        }
        INFO("base_url: {}", _base_url.c_str()); 
        _is_available = true;
        return true;
    }


    // Get接口
    bool MimoProvider::IsAvailable() {return _is_available;}
    std::string MimoProvider::GetModels() {return "mimo-v2.5-pro";}
    std::string MimoProvider::GetDesc() {
        return "由小米公司大模型Core团队开发的大语言模型。能够帮助你解答各种问题";
    }

    std::string MimoProvider::BuildRequestBody(Messages messages, Params request_params, bool isstream) {
        // 1. 读取请求参数
        // 如果有温度和最大token数，则使用指定的参数 否则使用默认值
        double temperature = 0.8;
        int max_tokens = 2048;
        if(request_params.find("temperature") != request_params.end()) {
            temperature = std::stod(request_params["temperature"]);
        }
        if(request_params.find("max_completion_tokens") != request_params.end()) {
            max_tokens = std::stoi(request_params["max_tokens"]);
        }
        // 2. 构建Json历史消息
        Json::Value message_array(Json::arrayValue);
        for(auto &message : messages) {
            Json::Value message_obj;
            message_obj["role"] = message._role;
            message_obj["content"] = message._content;
            message_array.append(message_obj);
        }
        // 3. 构建Json请求参数
        Json::Value request_obj;
        request_obj["model"] = "mimo-v2.5-pro";
        request_obj["messages"] = message_array;
        request_obj["temperature"] = temperature;
        request_obj["max_completion_tokens"] = max_tokens;
        request_obj["stream"] = isstream;
        // 4. 序列化请求参数
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        std::string request_str;
        request_str = Json::writeString(builder, request_obj);
        INFO("request_str: {}", request_str);
        return request_str;
    }

    std::string MimoProvider::SendMessage(Messages messages, Params request_params) {
        // 1. 先检测模型是否可用
        if(!_is_available) {
            ERR("MimoProvider::SendMessage() provider is not available");
            return "";
        }
        // 2. 构建请求正文
        std::string request_str = BuildRequestBody(messages, request_params, false);
        // 3. 构建client 发送POST请求
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
        INFO("path: {}", _base_url + "/chat/completions");
        if(!res) {
            ERR("MimoProvider::SendMessage() request failed");
            return "";
        }
        INFO("response status: {}", res->status);
        INFO("response_str: {}", res->body);

        if(res->status != 200) {
            ERR("MimoProvider::SendMessage() request failed");
            return "";
        }

        // 4. 如果是正常的响应，则解析响应内容
        // 先将res的body转换为json对象
        Json::Value root;
        Json::CharReaderBuilder read_builder;
        std::string errs;
        std::stringstream ss(res->body);
        bool parsingSuccessful = Json::parseFromStream(read_builder, ss, &root, &errs);
        if(!parsingSuccessful) {
            ERR("MimoProvider::SendMessage() parse json failed");
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
        ERR("MimoProvider::SendMessage() parse json failed");
        return "";
    }
    std::string MimoProvider::SendMessageStream(Messages messages, Params request_params, StreamCallback callback) {return "";}
    
}  // end namespace ai_chat_sdk