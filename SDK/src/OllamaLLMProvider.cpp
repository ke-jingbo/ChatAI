#include "../include/OllamaLLMProvider.h"
#include "../include/util/mylog.h"
#include <jsoncpp/json/json.h>
#include <httplib.h>

namespace ai_chat_sdk {
    // 初始化接口
    bool OllamaLLMProvider::InitProvider(Params &request_params) {
        // 查找模型名称
        auto model_name = request_params.find("model_name");
        if(model_name != request_params.end()) {
            _model_name = model_name->second;
        }
        else{
            ERR("OllamaLLMProvider::InitProvider() model_name is not found");
            return false;
        }
        // 查找模型描述
        auto model_desc = request_params.find("model_desc");
        if(model_desc != request_params.end()) {
            _model_desc = model_desc->second;
        }
        else{
            ERR("OllamaLLMProvider::InitProvider() model_desc is not found");
            return false;
        }
        // 查找base_url
        auto base_url = request_params.find("base_url");
        if(base_url != request_params.end()) {
            _base_url = base_url->second;
        }
        else{
            ERR("OllamaLLMProvider::InitProvider() base_url is not found");
            return false;
        }
        INFO("base_url: {}", _base_url.c_str());
        _is_available = true;
        return true;
    }

    // Get接口
    bool OllamaLLMProvider::IsAvailable() {return _is_available;}
    std::string OllamaLLMProvider::GetModels() {return _model_name;}
    std::string OllamaLLMProvider::GetDesc() {return _model_desc;}

    std::string OllamaLLMProvider::BuildRequestBody(Messages messages, Params request_params, bool isstream) {
        // 1. 读取请求参数
        // think keep_alive
        std::string keep_alive = "5m";
        double temperature = 1.0;
        bool think = true;
        if(request_params.find("temperature") != request_params.end()) {
            temperature = std::stod(request_params["temperature"]);
        }
        if(request_params.find("think") != request_params.end()) {
            if(request_params["think"] != "true") think = false;
        }
        // 2. 构建Json历史消息
        Json::Value messages_array(Json::arrayValue);
        for(auto &message : messages) {
            Json::Value message_obj;
            message_obj["role"] = message._role;
            message_obj["content"] = message._content;
            messages_array.append(message_obj);
        }
        // 3. 构建Json请求参数
        Json::Value request_obj;
        request_obj["model"] = _model_name;
        request_obj["messages"] = messages_array;
        request_obj["keep_alive"] = keep_alive;
        request_obj["think"] = think;
        Json::Value options_obj;
        options_obj["temperature"] = temperature;
        request_obj["options"] = options_obj;
        request_obj["stream"] = isstream;
        // 4. 序列化请求参数
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        std::string request_str;
        request_str = Json::writeString(builder, request_obj);
        DBG("request_str: {}", request_str);
        return request_str;
    }

    std::string OllamaLLMProvider::SendMessage(Messages messages, Params request_params) {
        // 1. 先检测模型是否可用
        if(!_is_available) {
            ERR("OllamaLLMProvider::SendMessage() provider is not available");
            return "";
        }
        // 2. 构建请求正文
        std::string request_str = BuildRequestBody(messages, request_params, false);
        // 3. 构建client 发送POST请求
        httplib::Client client(_base_url);
        client.set_connection_timeout(10, 0);  // 设置超时时间为10秒
        client.set_read_timeout(200, 0);        // 设置读取超时时间为200秒
        // 构建请求头
        httplib::Headers headers = {
            {"Content-Type", "application/json"}
        };
        // 发送POST请求
        auto res = client.Post("/api/chat", headers, request_str, "application/json");
        if(!res) {
            ERR("OllamaLLMProvider::SendMessage() request failed");
            return "";
        }
        if(res->status != 200) {
            ERR("OllamaLLMProvider::SendMessage() request failed");
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
            ERR("OllamaLLMProvider::SendMessage() parse json failed");
            return "";
        }
        // 检测是否有message元素
        if(root.isMember("message") && root["message"].isMember("content")) {
            std::string ret;
            // 如果有think元素，则解析think元素
            if(root["message"].isMember("thinking")) {
                std::string think = root["message"]["thinking"].asString();
                ret += ("modle think: " + think + "\r\n");
            }
            std::string content = root["message"]["content"].asString();
            ret += ("modle content: " + content);
            return ret;
        }
        ERR("OllamaLLMProvider::SendMessage() parse json failed");
        return "";
    }

    std::string OllamaLLMProvider::SendMessageStream(Messages messages, Params request_params, StreamCallback callback) {
        // 1. 判断模型是否可用
        if(!_is_available) {
            ERR("OllamaLLMProvider::SendMessageStream() provider is not available");
            return "";
        }
        // 2. 构建请求正文
        std::string request_str = BuildRequestBody(messages, request_params, true);
        // 3. 构建client 发送POST请求
        httplib::Client client(_base_url);
        client.set_connection_timeout(60, 0);  // 设置超时时间为30秒
        client.set_read_timeout(300, 0);        // 设置读取超时时间为120秒
        // 设置请求头
        httplib::Headers headers = {
            {"Content-Type", "application/json"},
            {"Accept", "text/x-ndjson"}
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
        req.path = "/api/chat";
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
            DBG("OllamaLLMProvider sendMessageStream buffer: {}", buffer);
            size_t pos = 0;
            while((pos = buffer.find("\n")) != std::string::npos) {
                std::string chunk = buffer.substr(0, pos);
                buffer.erase(0, pos + 1);  // 删除已处理的内容 包括回车换行符
                DBG("OllamaLLMProvider sendMessageStream chunk: {}", chunk);
                // 此时获得的即为一个Json串
                // 对该信息进行解析
                Json::Value root;
                Json::CharReaderBuilder read_builder;
                std::string errs;
                std::stringstream ss(chunk);
                bool parsingSuccessful = Json::parseFromStream(read_builder, ss, &root, &errs);
                if(!parsingSuccessful) {
                    ERR("OllamaLLMProvider::SendMessageStream() parse json failed");
                    return false;
                }
                if(root.isMember("message") && root["message"].isMember("content")) {
                    if(root["message"].isMember("thinking")) {
                        std::string think = root["message"]["thinking"].asString();
                        full_response += think;
                        DBG("OllamaLLMProvider sendMessageStream think: {}", think);
                        callback(think, false);
                    }
                    else {
                        std::string content = root["message"]["content"].asString();
                        full_response += content;
                        DBG("OllamaLLMProvider sendMessageStream content: {}", content);
                        callback(content, false);
                    }
                }
                if(root.isMember("done") && root["done"].asBool() == true) {
                    is_last = true;
                    if(root.isMember("done_reason")) finish_reason = root["done_reason"].asString();
                    callback("", true);
                    return true;
                }
                if(!root.isMember("message") && !root.isMember("done")) {
                    ERR("OllamaLLMProvider::SendMessageStream() parse json failed: {}", chunk);
                    return false;
                }
            }
            return true;
        };
        // 发送请求
        auto res = client.send(req);
        if(!res) {
            ERR("res err: {}", to_string(res.error()));
            ERR("OllamaLLMProvider::SendMessageStream() request failed");
            return "";
        }
        // 判断是否为完整请求
        if(is_last == false) {
            WARN("stream ended without [DONE] marker");
            callback("", true);
        }
        return full_response;
    }

}  // end namespace ai_chat_sdk