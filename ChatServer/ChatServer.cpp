#include "ChatServer.h"

namespace ai_chat_server {

    ChatServer::ChatServer(ChatServerConfig config) :_chat_sdk(config._db_name), _config(config) {
        // 初始化日志
        mylog::Logger::Init("chat_server", config._log_path, config.log_level);
        // 初始化sdk
        if(!_chat_sdk.InitLLMManager(config._provider_configs, config._models)) {
            ERR("ChatServer::ChatServer() init sdk failed");
            return;
        }
    }

    // 启动服务器
    bool ChatServer::Start() {
        if(_is_running.load()) {
            ERR("ChatServer::Start() server is already running");
            return false;
        }
        SetHttpRoute();
        _server.set_mount_point("/", "./www");
        // 另启线程启动服务器
        std::thread t([this]() {
            _server.listen(_config._host, _config._port);
        });
        _is_running.store(true);
        t.detach();
        return true;
    }

    // 停止服务器
    bool ChatServer::Stop() {
        if(!_is_running.load()) {
            ERR("ChatServer::Stop() server is not running");
            return false;
        }
        _is_running.store(false);
        _server.stop();
        return true;
    }

    // api接口
    // 构建错误响应
    void ChatServer::BuildERRResponse(httplib::Response &res, int status, const std::string &message) {
        Json::Value error_obj;
        error_obj["message"] = message;
        error_obj["success"] = false;
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        std::string request_str;
        std::string error_str = Json::writeString(builder, error_obj);
        res.set_content(message, "application/json");
        res.status = status;
    }
    // 创建新会话
    // POST /api/session
    void ChatServer::CreateNewSession(const httplib::Request& req, httplib::Response& res) {
        std::string request_body = req.body;
        Json::Value request_obj;
        Json::Reader reader;
        if(!reader.parse(request_body, request_obj)) {
            ERR("ChatServer::CreateNewSession() parse request body failed");
            BuildERRResponse(res, 400, "parse request body failed");
            return;
        }
        std::string model_name = request_obj["model_name"].asString();
        if(model_name.empty()) {
            ERR("ChatServer::CreateNewSession() model_name is empty");
            BuildERRResponse(res, 400, "model_name is empty");
            return;
        }
        // 创建会话
        std::string session_id = _chat_sdk.CreateSession(model_name);
        if(session_id.empty()) {
            ERR("ChatServer::CreateNewSession() create session failed");
            BuildERRResponse(res, 500, "create session failed");
            return;
        }
        // 创建会话成功
        Json::Value response_obj;
        response_obj["success"] = true;
        response_obj["message"] = "create session success";
        Json::Value data_obj;
        data_obj["session_id"] = session_id;
        Json::Value model_obj;
        model_obj["name"] = model_name;
        model_obj["temperature"] = _chat_sdk.GetModel(model_name)._config._temperature;
        model_obj["max_tokens"] = _chat_sdk.GetModel(model_name)._config._max_tokens;
        model_obj["think"] = _chat_sdk.GetModel(model_name)._config._think;
        model_obj["reasoning_effort"] = _chat_sdk.GetModel(model_name)._config._reasoning_effort;
        data_obj["model"] = model_obj;
        response_obj["data"] = data_obj;
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        std::string response_str = Json::writeString(builder, response_obj);
        res.set_content(response_str, "application/json");
        res.status = 200;
    }
    // 删除会话
    // DELETE /api/session/${session_id}
    void ChatServer::DeleteSession(const httplib::Request& req, httplib::Response& res) {
        std::string session_id = req.matches[1].str();
        _chat_sdk.DeleteSession(session_id);
        Json::Value response_obj;
        response_obj["success"] = true;
        response_obj["message"] = "delete session success";
        res.set_content(response_obj.toStyledString(), "application/json");
        res.status = 200;
    }
    // 获取会话列表
    // GET /api/sessions
    void ChatServer::GetSessionList(const httplib::Request& req, httplib::Response& res) {
        Json::Value response_obj;
        response_obj["success"] = true;
        response_obj["message"] = "get session list success";
        Json::Value data_obj;
        for(auto &session : _chat_sdk.GetSessions()) {
            Json::Value session_obj;
            session_obj["session_id"] = session;
            session_obj["model"] = _chat_sdk.GetSession(session)->_model_name;
            session_obj["start_time"] = static_cast<int64_t>(_chat_sdk.GetSession(session)->_start_time);
            session_obj["update_time"] = static_cast<int64_t>(_chat_sdk.GetSession(session)->_update_time);
            session_obj["message_count"] = _chat_sdk.GetSession(session)->_messages.size();
            session_obj["first_message"] = _chat_sdk.GetSession(session)->_messages[0]._content;
            data_obj.append(session_obj);
        }
        response_obj["data"] = data_obj;
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        std::string response_str = Json::writeString(builder, response_obj);
        res.set_content(response_str, "application/json");
        res.status = 200;
    }
    // 获取指定会话历史消息
    // GET /api/session/${session_id}/history
    void ChatServer::GetSessionHistory(const httplib::Request& req, httplib::Response& res) {
        std::string session_id = req.matches[1].str();
        if(session_id.empty()) {
            ERR("ChatServer::GetSessionHistory() session_id is empty");
            BuildERRResponse(res, 400, "session_id is empty");
            return;
        }
        Json::Value response_obj;
        response_obj["success"] = true;
        response_obj["message"] = "get session history success";
        Json::Value data_obj;
        for(auto &message : _chat_sdk.GetSession(session_id)->_messages) {
            Json::Value message_obj;
            message_obj["role"] = message._role;
            message_obj["content"] = message._content;
            message_obj["timestamp"] = static_cast<int64_t>(message._timestamp);
            message_obj["message_id"] = message._messageid;
            data_obj.append(message_obj);
        }
        response_obj["data"] = data_obj;
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        std::string response_str = Json::writeString(builder, response_obj);
        res.set_content(response_str, "application/json");
        res.status = 200;
    }
    // 获取可用模型
    // GET /api/models
    void ChatServer::GetModels(const httplib::Request& req, httplib::Response& res) {
        ai_chat_sdk::Models models = _chat_sdk.GetAvailableModels();
        if(models.empty()) {
            ERR("ChatServer::GetModels() get models failed");
            BuildERRResponse(res, 500, "get models failed");
            return;
        }
        Json::Value response_obj;
        response_obj["success"] = true;
        response_obj["message"] = "get models success";
        Json::Value data_obj;
        for(auto &model : models) {
            Json::Value model_obj;
            model_obj["name"] = model._name;
            model_obj["desc"] = model._desc;
            data_obj.append(model_obj);
        }
        response_obj["data"] = data_obj;
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        std::string response_str = Json::writeString(builder, response_obj);
        res.set_content(response_str, "application/json");
        res.status = 200;
    }
    // 更改当前会话的模型参数
    // POST /api/session/model_config
    void ChatServer::ChangeModelConfig(const httplib::Request& req, httplib::Response& res) {
        Json::Value request_obj;
        Json::Reader reader;
        if(!reader.parse(req.body, request_obj)) {
            ERR("ChatServer::ChangeModelConfig() parse request body failed");
            BuildERRResponse(res, 400, "parse request body failed");
            return;
        }
        // 获取更新参数
        std::string session_id = request_obj["session_id"].asString();
        std::string model_name = request_obj["model"].asString();
        double temperature = request_obj["temperature"].asDouble();
        int max_tokens = request_obj["max_tokens"].asInt();
        bool think = request_obj["think"].asBool();
        std::string reasoning_effort = request_obj["reasoning_effort"].asString();
        ai_chat_sdk::Params params;
        params["temperature"] = std::to_string(temperature);
        params["max_tokens"] = std::to_string(max_tokens);
        params["think"] = think ? "true" : "false";
        params["reasoning_effort"] = reasoning_effort;
        // 更新模型参数
        if(!_chat_sdk.UpdateSessionModelConfig(session_id, params)) {
            ERR("ChatServer::ChangeModelConfig() update session model config failed");
            BuildERRResponse(res, 500, "update session model config failed");
            return;
        }
        // 更新成功
        Json::Value response_obj;
        response_obj["success"] = true;
        response_obj["message"] = "change model config success";
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        std::string response_str = Json::writeString(builder, response_obj);
        res.set_content(response_str, "application/json");
        res.status = 200;
    }
    // 更改当前会话的模型
    // POST /api/session/model
    void ChatServer::ChangeModel(const httplib::Request& req, httplib::Response& res) {
        Json::Value request_obj;
        Json::Reader reader;
        if(!reader.parse(req.body, request_obj)) {
            ERR("ChatServer::ChangeModelConfig() parse request body failed");
            BuildERRResponse(res, 400, "parse request body failed");
            return;
        }
        // 获取更新模型名称
        std::string session_id = request_obj["session_id"].asString();
        std::string model_name = request_obj["model"].asString();
        // 更新模型
        if(!_chat_sdk.UpdateSessionModel(session_id, model_name)) {
            ERR("ChatServer::ChangeModel() update session model failed");
            BuildERRResponse(res, 500, "update session model failed");
            return;
        }
        // 更新成功
        Json::Value response_obj;
        response_obj["success"] = true;
        response_obj["message"] = "change model success";
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        std::string response_str = Json::writeString(builder, response_obj);
        res.set_content(response_str, "application/json");
        res.status = 200;
    }
    // 全量返回消息
    // POST /api/message
    void ChatServer::SendMessage(const httplib::Request& req, httplib::Response& res) {
        Json::Value request_obj;
        Json::Reader reader;
        if(!reader.parse(req.body, request_obj)) {
            ERR("ChatServer::SendMessage() parse request body failed");
            BuildERRResponse(res, 400, "parse request body failed");
            return;
        }
        // 获取会话id和消息
        std::string session_id = request_obj["session_id"].asString();
        std::string message = request_obj["message"].asString();
        // 发送消息
        std::string ai_message = _chat_sdk.SendMessage(session_id, message);
        if(ai_message.empty()) {
            ERR("ChatServer::SendMessage() send message failed");
            BuildERRResponse(res, 500, "send message failed");
            return;
        }
        // 发送成功
        Json::Value response_obj;
        response_obj["success"] = true;
        response_obj["message"] = "send message success";
        Json::Value data_obj;
        data_obj["response"] = ai_message;
        data_obj["session_id"] = session_id;
        response_obj["data"] = data_obj;
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        std::string response_str = Json::writeString(builder, response_obj);
        res.set_content(response_str, "application/json");
        res.status = 200;
    }
    // 流式返回消息
    // POST /api/message/async
    void ChatServer::SendMessageStream(const httplib::Request& req, httplib::Response& res) {
        Json::Value request_obj;
        Json::Reader reader;
        if(!reader.parse(req.body, request_obj)) {
            ERR("ChatServer::SendMessageStream() parse request body failed");
            BuildERRResponse(res, 400, "parse request body failed");
            return;
        }
        // 获取会话id和消息
        std::string session_id = request_obj["session_id"].asString();
        std::string message = request_obj["message"].asString();
        if(session_id.empty() || message.empty()) {
            ERR("ChatServer::SendMessageStream() session_id or message is empty");
            BuildERRResponse(res, 400, "session_id or message is empty");
            return;
        }
        res.set_header("Connection", "keep-alive");
        res.set_header("Cache-Control", "no-cache");
        res.set_header("Access-Control-Allow-Origin", "*");       // 允许跨域请求
        res.set_header("Access-Control-Allow-Headers", "*");      // 允许所有请求头
        res.set_chunked_content_provider("text/event-stream", 
            [this, session_id, message](size_t offset, httplib::DataSink &data_sink)->bool{
                auto write_callback = [&](const std::string &message, bool flag) {
                    // Json::valueToQuotedString: 对chunk进行Json转换，目的防止chunk中包含一些特殊字符来破坏数据格式，
                    // 比如：在chunk中包含了两个连续的换行，就会影响SSE数据格式
                    std::string chunk = "data: " + Json::valueToQuotedString(message.c_str()) + "\n\n";
                    // 用data_sink.write方法立即将数据发送到客户端
                    data_sink.write(chunk.c_str(), chunk.size());
                    if(flag == true) {
                        // 如果是最后一条消息，则关闭连接
                        std::string close_chunk = "data: [DONE]\n\n";
                        data_sink.write(close_chunk.c_str(), close_chunk.size());
                        data_sink.done();
                        return false;
                    }
                    return true;
                };
                // 先返回一个空数据块
                if(!write_callback("", false)) return false;
                // 发送消息
                // 阻塞发送消息，直到收到回调函数返回false
                _chat_sdk.SendMessage(session_id, message);
                return true;  // 当前数据块已经发送完毕
        });
    }

    // 设置路由
    void ChatServer::SetHttpRoute() {
        _server.Get("/api/sessions", 
            [this](const httplib::Request& req, httplib::Response& res) {
                GetSessionList(req, res);
        });
        _server.Get("/api/session/{session_id}/history", 
            [this](const httplib::Request& req, httplib::Response& res) {
                GetSessionHistory(req, res);
        });
        _server.Get("/api/models", 
            [this](const httplib::Request& req, httplib::Response& res) {
                GetModels(req, res);
        });
        _server.Post("/api/session", 
            [this](const httplib::Request& req, httplib::Response& res) {
                CreateNewSession(req, res);
        });
        _server.Delete("/api/session/{session_id}", 
            [this](const httplib::Request& req, httplib::Response& res) {
                DeleteSession(req, res);
        });
        _server.Get("/api/session/{session_id}/history", 
            [this](const httplib::Request& req, httplib::Response& res) {
                GetSessionHistory(req, res);
        });
        _server.Post("/api/session/model_config", 
            [this](const httplib::Request& req, httplib::Response& res) {
                ChangeModelConfig(req, res);
        });
        _server.Post("/api/session/model", 
            [this](const httplib::Request& req, httplib::Response& res) {
                ChangeModel(req, res);
        });
        _server.Post("/api/message", 
            [this](const httplib::Request& req, httplib::Response& res) {
                SendMessage(req, res);
        });
        _server.Post("/api/message/async", 
            [this](const httplib::Request& req, httplib::Response& res) {
                SendMessageStream(req, res);
        });
    }
} // namespace ai_chat_server