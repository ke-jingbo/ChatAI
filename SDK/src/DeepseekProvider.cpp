#include "../include/DeepseekProvider.h"
#include "../include/util/mylog.h"
#include <jsoncpp/json/json.h>
#include <httplib.h>

namespace ai_chat_sdk {
    // 初始化接口
    bool DeepseekProvider::InitProvider(Params &request_params) {
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
        INFO("base_url: {}", _base_url.c_str()); 
        _is_available = true;
        return true;
    }


    // Get接口
    bool DeepseekProvider::IsAvailable() {return _is_available;}
    std::string DeepseekProvider::GetModels() {return "deepseek-flash";}
    std::string DeepseekProvider::GetDesc() {
        return "DeepSeek 的轻量版快速版模型。响应速度更快、推理成本更低，适合处理日常、高频的对话任务";
    }


    std::string DeepseekProvider::BuildRequestBody(Messages messages, Params request_params, bool isstream) {
        // 1. 读取请求参数
        // 如果有温度和最大token数，则使用指定的参数 否则使用默认值
        double temperature = 0.8;
        int max_tokens = 2048;
        if(request_params.find("temperature") != request_params.end()) {
            temperature = std::stod(request_params["temperature"]);
        }
        if(request_params.find("max_tokens") != request_params.end()) {
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
        request_obj["model"] = "deepseek-flash";
        request_obj["messages"] = message_array;
        request_obj["temperature"] = temperature;
        request_obj["max_tokens"] = max_tokens;
        request_obj["stream"] = isstream;
        // 4. 序列化请求参数
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        std::string request_str;
        request_str = Json::writeString(builder, request_obj);
        INFO("request_str: {}", request_str);
        return request_str;
    }


    std::string DeepseekProvider::SendMessage(Messages messages, Params request_params) {
        // 1. 先检测模型是否可用
        if(!_is_available) {
            ERR("DeepseekProvider::SendMssage() provider is not available");
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

        // 4. 如果是正常的响应，则解析响应内容
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


    std::string DeepseekProvider::SendMessageStream(Messages messages, Params request_params, StreamCallback callback) {
        // 1. 判断模型是否可用
        if(!_is_available) {
            ERR("DeepseekProvider::SendMessageStream() provider is not available");
            return "";
        }
        // 2. 构建请求正文
        std::string request_str = BuildRequestBody(messages, request_params, true);
        // 3. 构建client 发送POST请求
        httplib::Client client(_base_url);
        client.set_connection_timeout(30, 0);  // 设置超时时间为30秒
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
        // 创建并设置请求
        httplib::Request req;
        req.path = "/chat/completions";
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
            DBG("DeepSeekProvider sendMessageStream buffer: {}", buffer);
            size_t pos = 0;
            while((pos = buffer.find("\n\n")) != std::string::npos) {
                std::string chunk = buffer.substr(0, pos);
                buffer.erase(0, pos + 2);  // 删除已处理的内容 包括回车换行符
                DBG("DeepSeekProvider sendMessageStream chunk: {}", chunk);
                if(chunk[0] == ':' || chunk.empty()) continue;  // :开头的是消息头，忽略
                if(chunk.substr(0, 6) == "data: ") {
                    std::string data = chunk.substr(6);
                    DBG("DeepSeekProvider sendMessageStream data: {}", data);
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
                        ERR("DeepseekProvider::SendMessageStream() parse json failed");
                        return false;
                    }
                    if(root.isMember("choices") && root["choices"].isArray() && !root["choices"].empty()
                        && root["choices"][0].isMember("delta") && root["choices"][0]["delta"].isMember("content")) {
                        std::string content = root["choices"][0]["delta"]["content"].asString();
                        callback(content, false);
                    }
                    else {
                        ERR("DeepseekProvider::SendMessageStream() parse json failed");
                        return false;
                    }
                }
            }
            return true;
        };
        // 发送请求
        auto res = client.send(req);
        if(!res) {
            ERR("DeepseekProvider::SendMessageStream() request failed");
            return "";
        }
        // 判断是否为完整请求
        if(is_last == false) {
            ERR("stream ended without [DONE] marker");
            callback("", true);
        }
        return full_response;
    }
} // end namespace ai_chat_sdk