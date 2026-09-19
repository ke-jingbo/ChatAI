#include "../include/MimoProvider.h"
#include "../include/util/mylog.h"
#include <jsoncpp/json/json.h>
#include <httplib.h>

namespace ai_chat_sdk {
    // 初始化接口
    bool MimoProvider::InitProvider(Params &provider_config) {
        // 查找api_key
        auto api_key = provider_config.find("api_key");
        if(api_key != provider_config.end()) {
            _api_key = api_key->second;
        }
        else{
            ERR("MimoProvider::InitProvider() api_key is not found");
            return false;
        }
        // 查找base_url
        auto base_url = provider_config.find("base_url");
        if(base_url != provider_config.end()) {
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
    std::string MimoProvider::GetProviderName() {return "MimoProvider";}
    std::string MimoProvider::GetDesc() {
        return "由小米公司大模型Core团队开发的大语言模型。能够帮助你解答各种问题";
    }

    // SendMessage接口
    std::string MimoProvider::BuildRequestBody(Model model, Messages messages, Params request_params, bool isstream) {
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
        request_obj["model"] = model._name;
        request_obj["messages"] = message_array;
        request_obj["temperature"] = temperature;
        request_obj["max_completion_tokens"] = max_tokens;
        request_obj["stream"] = isstream;
        // 4. 序列化请求参数
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        std::string request_str;
        request_str = Json::writeString(builder, request_obj);
        DBG("request_str: {}", request_str);
        return request_str;
    }

    std::string MimoProvider::SendMessage(Model model, Messages messages, Params request_params) {
        // 1. 先检测模型是否可用
        if(!_is_available) {
            ERR("MimoProvider::SendMessage() provider is not available");
            return "";
        }
        // 2. 构建请求正文
        std::string request_str = BuildRequestBody(model, messages, request_params, false);
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
        auto res = client.Post("/v1/chat/completions", headers, request_str, "application/json");
        if(!res) {
            ERR("MimoProvider::SendMessage() request failed");
            return "";
        }
        if(res->status != 200) {
            ERR("MimoProvider::SendMessage() request failed");
            ERR("response status: {}", res->status);
            ERR("response_str: {}", res->body);
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

    std::string MimoProvider::SendMessageStream(Model model, Messages messages, Params request_params, StreamCallback callback) {
        // 1. 判断模型是否可用
        if(!_is_available) {
            ERR("MimoProvider::SendMessageStream() provider is not available");
            return "";
        }
        // 2. 构建请求正文
        std::string request_str = BuildRequestBody(model, messages, request_params, true);
        // 3. 构建client 发送POST请求
        httplib::Client client(_base_url);
        client.set_connection_timeout(60, 0);  // 设置超时时间为30秒
        client.set_read_timeout(300, 0);        // 设置读取超时时间为120秒
        // 设置请求头
        httplib::Headers headers = {
            {"Content-Type", "application/json"},
            {"Authorization", "Bearer " + _api_key},
            {"Accept", "text/event-stream"}
        };
        // 流式处理相关变量
        std::string buffer;         // 临时缓存区
        bool is_last = false;       // 是否是最后一条消息
        bool is_err = false;        // 是否出错
        std::string err_msg;        // 错误信息
        int status_code = 0;        // HTTP状态码
        std::string full_response;  // 完整的响应内容
        std::string finish_reason;  // 完成原因
        // 创建并设置请求
        httplib::Request req;
        req.path = "/v1/chat/completions";
        req.method = "POST";
        req.headers = headers;
        req.body = request_str;
        // 设置接收应答处理函数
        req.response_handler = [&](const httplib::Response &res){
            status_code = res.status;
            if(status_code != 200) {
                is_err = true;
                err_msg = "HTTP status code: " + std::to_string(res.status);
                return false;
            }
            return true;
        };
        // 设置流式数据处理函数
        req.content_receiver = [&](const char* data, size_t len, size_t offset, size_t totallen) {
            if(is_err) return false;
            buffer.append(data, len);
            DBG("MimoProvider sendMessageStream buffer: {}", buffer);
            size_t pos = 0;
            while((pos = buffer.find("\n\n")) != std::string::npos) {
                std::string chunk = buffer.substr(0, pos);
                buffer.erase(0, pos + 2);  // 删除已处理的内容 包括回车换行符
                DBG("MimoProvider sendMessageStream chunk: {}", chunk);
                if(chunk[0] == ':' || chunk.empty()) continue;  // :开头的是消息头，忽略
                if(chunk.substr(0, 6) == "data: ") {
                    std::string data = chunk.substr(6);
                    DBG("MimoProvider sendMessageStream data: {}", data);
                    if(data.empty()) continue;
                    // 如果是DONE则表示所有数据块已发送完毕
                    if(data == "[DONE]") {
                        is_last = true;
                        callback("", true);
                        return true;
                    }
                    // 如果是数据块，则解析数据块
                    Json::Value root;
                    Json::CharReaderBuilder read_builder;
                    std::string errs;
                    std::stringstream ss(data);
                    bool parsingSuccessful = Json::parseFromStream(read_builder, ss, &root, &errs);
                    if(!parsingSuccessful) {
                        ERR("MimoProvider::SendMessageStream() parse json failed");
                        return false;
                    }
                    if(root.isMember("choices") && root["choices"].isArray()) {
                        if(!root["choices"].empty() && root["choices"][0].isMember("delta") 
                            && root["choices"][0]["delta"].isMember("content")) {
                            std::string content = root["choices"][0]["delta"]["content"].asString();
                            if(content.empty()) continue;
                            full_response += content;
                            callback(content, false);
                        }
                        if(!root["choices"].empty() && root["choices"][0].isMember("finish_reason") && !(root["choices"][0]["finish_reason"].asString().empty())) {
                            finish_reason = root["choices"][0]["finish_reason"].asString();
                            INFO("MimoProvider sendMessageStream finish_reason: {}", finish_reason);
                            is_last = true;
                            callback("", true);
                            return true;
                        }
                    }
                    else {
                        ERR("MimoProvider::SendMessageStream() parse json failed: {}", data);
                        ERR("MimoProvider::SendMessageStream() parse json failed");
                        return false;
                    }
                }
            }
            return true;
        };
        // 发送请求
        auto res = client.send(req);
        if(!res) {
            ERR("res err: {}", to_string(res.error()));
            ERR("MimoProvider::SendMessageStream() request failed");
            return "";
        }
        // 判断是否为完整请求
        if(is_last == false) {
            ERR("stream ended without [DONE] marker");
            callback("", true);
        }
        return full_response;
    }
    
}  // end namespace ai_chat_sdk