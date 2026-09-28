#include "../include/ChatServer.h"

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
    // 获取Cookie中的值
    std::string ChatServer::GetCookieValue(const httplib::Request &req, const std::string &key) {
        if(key.empty()) return "";
        std::string result;
        bool found = false;
        const std::size_t header_count = req.get_header_value_count("Cookie");
        for(std::size_t header_index = 0; header_index < header_count; ++header_index) {
            const std::string cookie_header =
                req.get_header_value("Cookie", "", header_index);
            std::size_t position = 0;
            while(position < cookie_header.size()) {
                const std::size_t separator = cookie_header.find(';', position);
                const std::size_t item_end = separator == std::string::npos
                    ? cookie_header.size()
                    : separator;
                std::size_t item_begin = position;
                while(item_begin < item_end && (cookie_header[item_begin] == ' '
                          || cookie_header[item_begin] == '\t')) {
                    ++item_begin;
                }
                const std::size_t equal = cookie_header.find('=', item_begin);
                if(equal != std::string::npos && equal < item_end) {
                    std::size_t name_end = equal;
                    while(name_end > item_begin
                          && (cookie_header[name_end - 1] == ' '
                              || cookie_header[name_end - 1] == '\t')) {
                        --name_end;
                    }
                    if(cookie_header.compare(item_begin, name_end-item_begin, key) == 0) {
                        // 同名Cookie会造成身份凭据歧义，直接视为无效请求。
                        if(found) return "";
                        result = cookie_header.substr(equal + 1,item_end - equal - 1);
                        found = true;
                    }
                }
                if(separator == std::string::npos) break;
                position = separator + 1;
            }
        }
        return found ? result : "";
    }
    // 获取用户id
    std::string ChatServer::GetUserId(const httplib::Request &req) {
        return _chat_sdk.GetUserId(GetCookieValue(req, "cookie_id"));
    }
    // 创建cookie id
    std::string ChatServer::CreateCookieId() {
            // 使用 OpenSSL 的密码学安全随机数生成器产生 256 bit 随机数据。
            std::array<unsigned char, 32> random_bytes{};
            if(RAND_bytes(random_bytes.data(),
                          static_cast<int>(random_bytes.size())) != 1) {
                // 随机数生成失败时不能退化为时间戳或空字符串，否则 Cookie 可被预测。
                throw std::runtime_error("failed to generate secure cookie id");
            }

            // 十六进制字符仅包含 0-9 和 a-f，可以安全地直接作为 Cookie 值。
            static constexpr char hex_chars[] = "0123456789abcdef";
            std::string cookie_id;
            cookie_id.resize(random_bytes.size() * 2);
            for(std::size_t i = 0; i < random_bytes.size(); ++i) {
                cookie_id[i * 2] = hex_chars[random_bytes[i] >> 4];
                cookie_id[i * 2 + 1] = hex_chars[random_bytes[i] & 0x0f];
            }
            return cookie_id;
    }


    // 用户相关操作
    // 邮箱验证码申请
    // POST /api/auth/email/code
    void ChatServer::SendEmailVerificationCode(const httplib::Request& req, httplib::Response& res) {
        Json::Value request_obj;
        Json::Reader reader;
        if(!reader.parse(req.body, request_obj)) {
            ERR("ChatServer::SendEmailVerificationCode() parse request body failed");
            BuildERRResponse(res, 400, "parse request body failed");
            return;
        }
        // 获取相关信息
        std::string email;
        if(!request_obj.isMember("email") || !request_obj["email"].isString()
            || !request_obj.isMember("purpose") || !request_obj["purpose"].isString()) {
            ERR("ChatServer::SendEmailVerificationCode() parse request body failed");
            BuildERRResponse(res, 400, "parse request body failed");
            return;
        }
        email = request_obj["email"].asString();
        std::string purpose = request_obj["purpose"].asString();
        // 发送验证码
        SendCodeResult result = _email_verification_service.SendCode(email, purpose);
        if(result.status == SendCodeStatus::TooFrequent) {
            ERR("ChatServer::SendEmailVerificationCode() send email verification code failed");
            Json::Value error_obj;
            error_obj["message"] = "send email verification code failed";
            error_obj["success"] = false;
            Json::Value data_obj;
            data_obj["retry_after"] = result.retry_after.count();
            error_obj["data"] = data_obj;
            Json::StreamWriterBuilder builder;
            builder["indentation"] = "";
            std::string request_str;
            std::string error_str = Json::writeString(builder, error_obj);
            res.set_content(error_str, "application/json");
            res.status = 429;
            return;
        }
        if(result.status != SendCodeStatus::Success) {
            ERR("ChatServer::SendEmailVerificationCode() send email verification code failed");
            BuildERRResponse(res, 500, "send email verification code failed");
            return;
        }
        Json::Value response_obj;
        response_obj["success"] = true;
        response_obj["message"] = "send email verification code success";
        Json::Value data_obj;
        data_obj["retry_after"] = result.retry_after.count();
        response_obj["data"] = data_obj;
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        std::string response_str = Json::writeString(builder, response_obj);
        res.set_content(response_str, "application/json");
        res.status = 200;
        INFO("SendEmailVerificationCode() success: {}", email);
    }
    // 邮箱验证码验证
    // POST /api/auth/email/verify
    void ChatServer::VerifyEmail(const httplib::Request& req, httplib::Response& res) {
        Json::Value request_obj;
        Json::Reader reader;
        if(!reader.parse(req.body, request_obj)) {
            ERR("ChatServer::VerifyEmail() parse request body failed");
            BuildERRResponse(res, 400, "parse request body failed");
            return;
        }
        // 获取相关信息
        std::string email;
        std::string code;
        std::string purpose;
        if(!request_obj.isMember("email") || !request_obj["email"].isString()
            || !request_obj.isMember("code") || !request_obj["code"].isString()
            || !request_obj.isMember("purpose") || !request_obj["purpose"].isString()) {
            ERR("ChatServer::VerifyEmail() parse request body failed");
            BuildERRResponse(res, 400, "parse request body failed");
            return;
        }
        email = request_obj["email"].asString();
        code = request_obj["code"].asString();
        purpose = request_obj["purpose"].asString();
        // 验证验证码
        VerifyCodeResult result = _email_verification_service.VerifyCode(email, purpose, code);
        if(result != VerifyCodeResult::Success) {
            ERR("ChatServer::VerifyEmail() verify email verification code failed");
            BuildERRResponse(res, 400, "verify email verification code failed");
            return;
        }
        Json::Value response_obj;
        response_obj["success"] = true;
        response_obj["message"] = "verify email verification code success";
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        std::string response_str = Json::writeString(builder, response_obj);
        res.set_content(response_str, "application/json");
        res.status = 200;
        INFO("VerifyEmail() success: {}", email);
    }
    // 注册用户
    // POST /api/auth/register
    void ChatServer::RegisterUser(const httplib::Request& req, httplib::Response& res) {
        Json::Value request_obj;
        Json::Reader reader;
        if(!reader.parse(req.body, request_obj)) {
            ERR("ChatServer::RegisterUser() parse request body failed");
            BuildERRResponse(res, 400, "parse request body failed");
            return;
        }
        // 获取相关信息
        std::string email;
        std::string user_name;
        std::string password;
        std::string code;
        std::string purpose = "register";
        std::string cookie_id = CreateCookieId();
        std::string user_avatar_path = "/images/avatar.png";
        if(!request_obj.isMember("email") || !request_obj["email"].isString()
            || !request_obj.isMember("user_name") || !request_obj["user_name"].isString()
            || !request_obj.isMember("password") || !request_obj["password"].isString()
            || !request_obj.isMember("code") || !request_obj["code"].isString()
            || !request_obj.isMember("purpose") || !request_obj["purpose"].isString()) {
            ERR("ChatServer::RegisterUser() parse request body failed");
            BuildERRResponse(res, 400, "parse request body failed");
            return;
        }
        email = request_obj["email"].asString();
        user_name = request_obj["user_name"].asString();
        password = request_obj["password"].asString();
        code = request_obj["code"].asString();
        purpose = request_obj["purpose"].asString();
        // 验证验证码
        VerifyCodeResult result = _email_verification_service.VerifyCode(email, purpose, code);
        if(result != VerifyCodeResult::Success) {
            ERR("ChatServer::VerifyEmail() verify email verification code failed");
            BuildERRResponse(res, 400, "verify email verification code failed");
            return;
        }
        // 注册用户
        if(_chat_sdk.CreateUser(user_name, user_avatar_path, email, password, cookie_id) == "") {
            ERR("ChatServer::RegisterUser() register user failed");
            BuildERRResponse(res, 500, "register user failed");
            return;
        }
        // 注册成功
        Json::Value response_obj;
        response_obj["success"] = true;
        response_obj["message"] = "register user success";
        Json::Value data_obj;
        Json::Value user_obj;
        user_obj["user_id"] = _chat_sdk.GetUserId(cookie_id);
        user_obj["user_name"] = user_name;
        user_obj["email"] = email;
        user_obj["avatar_path"] = user_avatar_path;
        user_obj["create_time"] = static_cast<int64_t>(time(nullptr));
        data_obj["user"] = user_obj;
        response_obj["data"] = data_obj;
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        std::string response_str = Json::writeString(builder, response_obj);
        res.set_content(response_str, "application/json");
        res.set_header("Set-Cookie", "cookie_id=" + cookie_id + "; Path=/; HttpOnly; SameSite=Lax; Max-Age=2592000");
        res.status = 200;
        INFO("RegisterUser() success {}:{}", user_name, email);
    }
    // 登录用户并设置Cookie
    // POST /api/auth/login
    void ChatServer::LoginUser(const httplib::Request& req, httplib::Response& res) {
        Json::Value request_obj;
        Json::Reader reader;
        if(!reader.parse(req.body, request_obj)) {
            ERR("ChatServer::LoginUser() parse request body failed");
            BuildERRResponse(res, 400, "parse request body failed");
            return;
        }
        // 获取相关信息
        std::string email;
        std::string password;
        if(!request_obj.isMember("email") || !request_obj["email"].isString()
            || !request_obj.isMember("password") || !request_obj["password"].isString()) {
            ERR("ChatServer::LoginUser() parse request body failed");
            BuildERRResponse(res, 400, "parse request body failed");
            return;
        }
        email = request_obj["email"].asString();
        password = request_obj["password"].asString();
        // 登录用户
        auto user = _chat_sdk.LoginUser(email, password);
        if (user == nullptr) {
            ERR("ChatServer::LoginUser() failed");
            BuildERRResponse(res, 400, "login failed");
            return;
        }
        // 登录成功
        Json::Value response_obj;
        response_obj["success"] = true;
        response_obj["message"] = "login success";
        Json::Value data_obj;
        data_obj["user_id"] = user->_user_id;
        data_obj["user_name"] = user->_user_name;
        data_obj["email"] = user->_email;
        data_obj["avatar_path"] = user->_user_avatar_path;
        data_obj["create_time"] = static_cast<int64_t>(user->_create_time);
        response_obj["data"] = data_obj;
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        std::string response_str = Json::writeString(builder, response_obj);
        res.set_content(response_str, "application/json");
        res.set_header("Set-Cookie", "cookie_id=" + user->_cookie_id + "; Path=/; HttpOnly; SameSite=Lax; Max-Age=2592000");
        res.status = 200;
        INFO("LoginUser() success: {}", email);
    }
    // 登出用户并清除Cookie
    // POST /api/auth/logout
    void ChatServer::LogoutUser(const httplib::Request& req, httplib::Response& res) {
        std::string user_id = GetUserId(req);
        if(user_id.empty()) {
            ERR("ChatServer::LogoutUser() user_id is empty");
            BuildERRResponse(res, 401, "user_id is empty");
            return;
        }
        _chat_sdk.LogoutUser(user_id);
        Json::Value response_obj;
        response_obj["success"] = true;
        response_obj["message"] = "logout success";
        res.set_content(response_obj.toStyledString(), "application/json");
        res.set_header("Set-Cookie", "cookie_id=; Path=/; HttpOnly; SameSite=Lax; Max-Age=0");
        res.status = 200;
        INFO("LogoutUser() success: {}", user_id);
    }
    // 获取用户信息
    // GET /api/auth/info
    void ChatServer::GetUserInfo(const httplib::Request& req, httplib::Response& res) {
        std::string user_id = GetUserId(req);
        if(user_id.empty()) {
            ERR("ChatServer::GetUserInfo() user_id is empty");
            BuildERRResponse(res, 401, "user_id is empty");
            return;
        }
        std::shared_ptr<ai_chat_sdk::User> user = _chat_sdk.GetUser(user_id);
        if(user == nullptr) {
            ERR("ChatServer::GetUserInfo() user not found: {}", user_id);
            BuildERRResponse(res, 404, "user not found");
            return;
        }
        Json::Value response_obj;
        response_obj["success"] = true;
        response_obj["message"] = "get user info success";
        Json::Value data_obj;
        data_obj["user_id"] = user->_user_id;
        data_obj["user_name"] = user->_user_name;
        data_obj["email"] = user->_email;
        data_obj["avatar_path"] = user->_user_avatar_path;
        data_obj["create_time"] = static_cast<int64_t>(user->_create_time);
        response_obj["data"] = data_obj;
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        std::string response_str = Json::writeString(builder, response_obj);
        res.set_content(response_str, "application/json");
        res.status = 200;
        INFO("GetUserInfo() success: {}", user_id);
    }
    // 更改用户头像
    // POST /api/user/avatar
    void ChatServer::ChangeUserAvatar(const httplib::Request& req, httplib::Response& res) {
        std::string user_id = GetUserId(req);
        if(user_id.empty()) {
            ERR("ChatServer::ChangeUserAvatar() user_id is empty");
            BuildERRResponse(res, 401, "user_id is empty");
            return;
        }
        if(!req.has_header("Content-Type") 
            || (req.get_header_value("Content-Type") != "image/png" 
            && req.get_header_value("Content-Type") != "image/jpeg")) {
            ERR("ChatServer::ChangeUserAvatar() Content-Type is not image/png or image/jpeg");
            BuildERRResponse(res, 400, "Content-Type is not image/png or image/jpeg");
            return;
        }
        std::string mime = (req.get_header_value("Content-Type")=="image/png")?"png":"jpeg";
        if(req.body.size() > 1024 * 1024 * 5) {
            ERR("ChatServer::ChangeUserAvatar() avatar size is too large");
            BuildERRResponse(res, 400, "avatar size is too large");
            return;
        }
        std::string avatar_path = "./www/images/avatar_" + user_id + "." + mime;
        // 更新用户头像
        if(!_chat_sdk.UpdateUserAvatar(user_id, avatar_path, std::make_shared<std::string>(req.body))) {
            ERR("ChatServer::ChangeUserAvatar() update user avatar failed");
            BuildERRResponse(res, 500, "update user avatar failed");
            return;
        }
        // 更新成功
        Json::Value response_obj;
        response_obj["success"] = true;
        response_obj["message"] = "change user avatar success";
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        std::string response_str = Json::writeString(builder, response_obj);
        res.set_content(response_str, "application/json");
        res.status = 200;
        INFO("ChangeUserAvatar() success: {}", user_id);
    }
    // 更改用户名称
    // POST /api/user/name
    void ChatServer::ChangeUserName(const httplib::Request& req, httplib::Response& res) {
        std::string user_id = GetUserId(req);
        if(user_id.empty()) {
            ERR("ChatServer::ChangeUserName() user_id is empty");
            BuildERRResponse(res, 401, "user_id is empty");
            return;
        }
        Json::Value request_obj;
        Json::Reader reader;
        if(!reader.parse(req.body, request_obj)) {
            ERR("ChatServer::ChangeUserName() parse request body failed");
            BuildERRResponse(res, 400, "parse request body failed");
            return;
        }
        std::string user_name;
        if(request_obj.isMember("user_name") || !request_obj["user_name"].isString())
            user_name = request_obj["user_name"].asString();
        // 更新用户名称
        if(!_chat_sdk.UpdateUserName(user_id, user_name)) {
            ERR("ChatServer::ChangeUserName() update user name failed");
            BuildERRResponse(res, 500, "update user name failed");
            return;
        }
        // 更新成功
        Json::Value response_obj;
        response_obj["success"] = true;
        response_obj["message"] = "change user name success";
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        std::string response_str = Json::writeString(builder, response_obj);
        res.set_content(response_str, "application/json");
        res.status = 200;
    }
    // 更改用户邮箱
    // POST /api/user/email
    void ChatServer::ChangeUserEmail(const httplib::Request& req, httplib::Response& res) {
        std::string user_id = GetUserId(req);
        if(user_id.empty()) {
            ERR("ChatServer::ChangeUserEmail() user_id is empty");
            BuildERRResponse(res, 401, "user_id is empty");
            return;
        }
        Json::Value request_obj;
        Json::Reader reader;
        if(!reader.parse(req.body, request_obj)) {
            ERR("ChatServer::ChangeUserEmail() parse request body failed");
            BuildERRResponse(res, 400, "parse request body failed");
            return;
        }
        std::string email;
        if(request_obj.isMember("email") || !request_obj["email"].isString()) 
            email = request_obj["email"].asString();
        // 更新用户邮箱
        if(!_chat_sdk.UpdateUserEmail(user_id, email)) {
            ERR("ChatServer::ChangeUserEmail() update user email failed");
            BuildERRResponse(res, 500, "update user email failed");
            return;
        }
        // 更新成功
        Json::Value response_obj;
        response_obj["success"] = true;
        response_obj["message"] = "change user email success";
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        std::string response_str = Json::writeString(builder, response_obj);
        res.set_content(response_str, "application/json");
        res.status = 200;
    }
    // 更改用户密码
    // POST /api/user/password
    void ChatServer::ChangeUserPassword(const httplib::Request& req, httplib::Response& res) {
        std::string user_id = GetUserId(req);
        if(user_id.empty()) {
            ERR("ChatServer::ChangeUserPassword() user_id is empty");
            BuildERRResponse(res, 401, "user_id is empty");
            return;
        }
        Json::Value request_obj;
        Json::Reader reader;
        if(!reader.parse(req.body, request_obj)) {
            ERR("ChatServer::ChangeUserPassword() parse request body failed");
            BuildERRResponse(res, 400, "parse request body failed");
            return;
        }
        std::string password;
        if(request_obj.isMember("password") || !request_obj["password"].isString())
            password = request_obj["password"].asString();
        // 更新用户密码
        if(!_chat_sdk.UpdateUserPassword(user_id, password)) {
            ERR("ChatServer::ChangeUserPassword() update user password failed");
            BuildERRResponse(res, 500, "update user password failed");
            return;
        }
        // 更新成功
        Json::Value response_obj;
        response_obj["success"] = true;
        response_obj["message"] = "change user password success";
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        std::string response_str = Json::writeString(builder, response_obj);
        res.set_content(response_str, "application/json");
        res.status = 200;
    }
    // 忘记密码
    // POST /api/auth/forget_password
    void ChatServer::ForgetUserPassword(const httplib::Request& req, httplib::Response& res) {
        Json::Value request_obj;
        Json::Reader reader;
        if(!reader.parse(req.body, request_obj)) {
            ERR("ChatServer::ForgetUserPassword() parse request body failed");
            BuildERRResponse(res, 400, "parse request body failed");
            return;
        }
        // 获取相关信息
        std::string email;
        std::string code;
        std::string password;
        if(!request_obj.isMember("email") || !request_obj["email"].isString()
            || !request_obj.isMember("code") || !request_obj["code"].isString()
            || !request_obj.isMember("password") || !request_obj["password"].isString()) {
            ERR("ChatServer::ForgetUserPassword() parse request body failed");
            BuildERRResponse(res, 400, "parse request body failed");
            return;
        }
        email = request_obj["email"].asString();
        code = request_obj["code"].asString();
        password = request_obj["password"].asString();

        // 忘记密码接口不接受客户端指定用途，避免验证码被跨业务使用。
        // 验证和修改必须在同一个后端请求中按顺序执行，防止绕过前端直接改密。
        const VerifyCodeResult verify_result =
            _email_verification_service.VerifyCode(email, "forget_password", code);
        switch(verify_result) {
        case VerifyCodeResult::Success:
            break;
        case VerifyCodeResult::NotFound:
            BuildERRResponse(res, 404, "verification code not found");
            return;
        case VerifyCodeResult::Expired:
            BuildERRResponse(res, 410, "verification code expired");
            return;
        case VerifyCodeResult::Incorrect:
            BuildERRResponse(res, 400, "verification code incorrect");
            return;
        case VerifyCodeResult::TooManyAttempts:
            BuildERRResponse(res, 429, "too many verification attempts");
            return;
        case VerifyCodeResult::AlreadyUsed:
            BuildERRResponse(res, 409, "verification code already used");
            return;
        case VerifyCodeResult::InvalidArgument:
            BuildERRResponse(res, 400, "invalid verification parameters");
            return;
        case VerifyCodeResult::InternalError:
            BuildERRResponse(res, 500, "verify email failed");
            return;
        }

        // 验证码成功消费后，才允许 SDK 更新对应邮箱的密码。
        if(!_chat_sdk.ForgetUserPassword(email, password)) {
            ERR("ChatServer::ForgetUserPassword() forget user password failed");
            BuildERRResponse(res, 500, "forget user password failed");
            return;
        }
        Json::Value response_obj;
        response_obj["success"] = true;
        response_obj["message"] = "forget user password success";
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        std::string response_str = Json::writeString(builder, response_obj);
        res.set_content(response_str, "application/json");
        res.status = 200;
    }

    
    // 会话相关操作
    // 创建新会话
    // POST /api/session
    void ChatServer::CreateNewSession(const httplib::Request& req, httplib::Response& res) {
        std::string user_id = GetUserId(req);
        if(user_id.empty()) {
            ERR("ChatServer::CreateNewSession() user_id is empty");
            BuildERRResponse(res, 401, "user_id is empty");
            return;
        }
        std::string request_body = req.body;
        Json::Value request_obj;
        Json::Reader reader;
        if(!reader.parse(request_body, request_obj)) {
            ERR("ChatServer::CreateNewSession() parse request body failed");
            BuildERRResponse(res, 400, "parse request body failed");
            return;
        }
        Json::Value model_obj;
        std::string model_name;
        double temperature;
        int max_tokens;
        bool think;
        std::string reasoning_effort;
        std::string session_name = "new session";
        if(!request_obj.isMember("model") || !request_obj["model"].isObject()) {
            ERR("ChatServer::CreateNewSession() parse request body failed");
            BuildERRResponse(res, 400, "parse request body failed");
            return;
        }
        model_obj = request_obj["model"];
        if(!model_obj.isMember("model_name") || !model_obj["model_name"].isString()
            || !model_obj.isMember("temperature") || !model_obj["temperature"].isDouble()
            || !model_obj.isMember("max_tokens") || !model_obj["max_tokens"].isInt()
            || !model_obj.isMember("think") || !model_obj["think"].isBool()
            || !model_obj.isMember("reasoning_effort") || !model_obj["reasoning_effort"].isString()) {
            ERR("ChatServer::CreateNewSession() parse model body failed");
            BuildERRResponse(res, 400, "parse model body failed");
            return;
        }
        if(!request_obj.isMember("session_name") || !request_obj["session_name"].isString()) {
            ERR("ChatServer::CreateNewSession() parse request body failed");
            BuildERRResponse(res, 400, "parse request body failed");
            return;
        }
        session_name = request_obj["session_name"].asString();
        model_name = model_obj["model_name"].asString();
        temperature = model_obj["temperature"].asDouble();
        max_tokens = model_obj["max_tokens"].asInt();
        think = model_obj["think"].asBool();
        reasoning_effort = model_obj["reasoning_effort"].asString();
        if(model_name.empty()) {
            ERR("ChatServer::CreateNewSession() model_name is empty");
            BuildERRResponse(res, 400, "model_name is empty");
            return;
        }
        // 创建会话
        std::string session_id = _chat_sdk.CreateSession(user_id, model_name, session_name);
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
        data_obj["session_name"] = session_name;
        Json::Value response_model_obj;
        response_model_obj["model"] = model_name;
        response_model_obj["temperature"] = _chat_sdk.GetModel(model_name)._config._temperature;
        response_model_obj["max_tokens"] = _chat_sdk.GetModel(model_name)._config._max_tokens;
        response_model_obj["think"] = _chat_sdk.GetModel(model_name)._config._think;
        response_model_obj["reasoning_effort"] = _chat_sdk.GetModel(model_name)._config._reasoning_effort;
        data_obj["model"] = response_model_obj;
        response_obj["data"] = data_obj;
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        std::string response_str = Json::writeString(builder, response_obj);
        res.set_content(response_str, "application/json");
        res.status = 200;
        INFO("CreateNewSession() success: {}", session_id);
    }
    // 删除会话
    // DELETE /api/session/${session_id}
    void ChatServer::DeleteSession(const httplib::Request& req, httplib::Response& res) {
        std::string session_id = req.matches[1].str();
        std::string user_id = GetUserId(req);
        if(user_id.empty()) {
            ERR("ChatServer::DeleteSession() user_id is empty");
            BuildERRResponse(res, 401, "user_id is empty");
            return;
        }
        _chat_sdk.DeleteSession(user_id, session_id);
        Json::Value response_obj;
        response_obj["success"] = true;
        response_obj["message"] = "delete session success";
        res.set_content(response_obj.toStyledString(), "application/json");
        res.status = 200;
        INFO("DeleteSession() success: {}", session_id);
    }
    // 删除所有会话
    // DELETE /api/sessions
    void ChatServer::DeleteAllSession(const httplib::Request& req, httplib::Response& res) {
        std::string user_id = GetUserId(req);
        _chat_sdk.ClearAllSessions(user_id);
        Json::Value response_obj;
        response_obj["success"] = true;
        response_obj["message"] = "delete all session success";
        res.set_content(response_obj.toStyledString(), "application/json");
        res.status = 200;
        INFO("DeleteAllSession() success");
    }
    // 获取会话列表
    // GET /api/sessions
    void ChatServer::GetSessionList(const httplib::Request& req, httplib::Response& res) {
        std::string user_id = GetUserId(req);
        if(user_id.empty()) {
            ERR("ChatServer::GetSessionList() user_id is empty");
            BuildERRResponse(res, 401, "user_id is empty");
            return;
        }
        Json::Value response_obj;
        response_obj["success"] = true;
        response_obj["message"] = "get session list success";
        Json::Value data_obj;
        for(auto &session : _chat_sdk.GetSessions(user_id)) {
            auto session_info = _chat_sdk.GetSession(user_id, session);
            Json::Value session_obj;
            session_obj["session_id"] = session;
            session_obj["session_name"] = session_info->_session_name;
            session_obj["model"] = session_info->_model_name;
            session_obj["start_time"] = static_cast<int64_t>(session_info->_start_time);
            session_obj["update_time"] = static_cast<int64_t>(session_info->_update_time);
            session_obj["message_count"] = session_info->_messages.size();
            const auto model = _chat_sdk.GetModel(session_info->_model_name);
            Json::Value model_config;
            model_config["model"] = model._name;
            model_config["temperature"] = model._config._temperature;
            model_config["max_tokens"] = model._config._max_tokens;
            model_config["think"] = model._config._think;
            model_config["reasoning_effort"] = model._config._reasoning_effort;
            session_obj["model_config"] = model_config;

            // 历史消息当前按时间倒序返回，因此显式查找最早的用户消息作为会话名称。
            const ai_chat_sdk::Message *first_user_message = nullptr;
            for(const auto &message : session_info->_messages) {
                if(message._role != "user") continue;
                if(first_user_message == nullptr
                    || message._timestamp < first_user_message->_timestamp
                    || (message._timestamp == first_user_message->_timestamp
                        && message._messageid < first_user_message->_messageid)) {
                    first_user_message = &message;
                }
            }
            session_obj["first_message"] = first_user_message == nullptr
                ? "new session"
                : first_user_message->_content;
            data_obj.append(session_obj);
        }
        response_obj["data"] = data_obj;
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        std::string response_str = Json::writeString(builder, response_obj);
        res.set_content(response_str, "application/json");
        res.status = 200;
        INFO("GetSessionList() success");
    }
    // 获取指定会话历史消息
    // GET /api/session/${session_id}/history
    void ChatServer::GetSessionHistory(const httplib::Request& req, httplib::Response& res) {
        std::string user_id = GetUserId(req);
        if(user_id.empty()) {
            ERR("ChatServer::GetSessionHistory() user_id is empty");
            BuildERRResponse(res, 401, "user_id is empty");
            return;
        }
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
        if(_chat_sdk.GetSession(user_id, session_id) == nullptr) {
            ERR("ChatServer::GetSessionHistory() session is not found: {}", session_id);
            BuildERRResponse(res, 404, "session is not found");
            return;
        }
        for(auto &message : _chat_sdk.GetSession(user_id, session_id)->_messages) {
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
        INFO("GetSessionHistory() success: {}", session_id);
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
            Json::Value model_config_obj;
            model_config_obj["temperature"] = model._config._temperature;
            model_config_obj["max_tokens"] = model._config._max_tokens;
            model_config_obj["think"] = model._config._think;
            model_config_obj["reasoning_effort"] = model._config._reasoning_effort;
            model_obj["config"] = model_config_obj;
            data_obj.append(model_obj);
        }
        response_obj["data"] = data_obj;
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        std::string response_str = Json::writeString(builder, response_obj);
        res.set_content(response_str, "application/json");
        res.status = 200;
        INFO("GetModels() success");
    }
    // 更改当前会话的名称
    // POST /api/session/name
    void ChatServer::ChangeSessionName(const httplib::Request& req, httplib::Response& res) {
        std::string user_id = GetUserId(req);
        if(user_id.empty()) {
            ERR("ChatServer::ChangeSessionName() user_id is empty");
            BuildERRResponse(res, 401, "user_id is empty");
            return;
        }
        Json::Value request_obj;
        Json::Reader reader;
        if(!reader.parse(req.body, request_obj)) {
            ERR("ChatServer::ChangeSessionName() parse request body failed");
            BuildERRResponse(res, 400, "parse request body failed");
            return;
        }
        // 获取更新名称
        std::string session_id = request_obj["session_id"].asString();
        std::string session_name = request_obj["session_name"].asString();
        // 更新名称
        if(!_chat_sdk.UpdateSessionName(user_id, session_id, session_name)) {
            ERR("ChatServer::ChangeSessionName() update session name failed");
            BuildERRResponse(res, 500, "update session name failed");
            return;
        }
        // 更新成功
        Json::Value response_obj;
        response_obj["success"] = true;
        response_obj["message"] = "change session name success";
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        std::string response_str = Json::writeString(builder, response_obj);
        res.set_content(response_str, "application/json");
        res.status = 200;
        INFO("ChangeSessionName() success: {}, {}", session_id, session_name);
    }
    // 更改当前会话的模型参数
    // POST /api/session/model_config
    void ChatServer::ChangeModelConfig(const httplib::Request& req, httplib::Response& res) {
        std::string user_id = GetUserId(req);
        if(user_id.empty()) {
            ERR("ChatServer::ChangeModelConfig() user_id is empty");
            BuildERRResponse(res, 401, "user_id is empty");
            return;
        }
        Json::Value request_obj;
        Json::Reader reader;
        if(!reader.parse(req.body, request_obj)) {
            ERR("ChatServer::ChangeModelConfig() parse request body failed");
            BuildERRResponse(res, 400, "parse request body failed");
            return;
        }
        // 获取更新参数
        std::string session_id;
        std::string model_name;
        double temperature;
        int max_tokens;
        bool think;
        std::string reasoning_effort;
        if(!request_obj.isMember("session_id") || !request_obj["session_id"].isString()
            || !request_obj.isMember("model") || !request_obj["model"].isString()
            || !request_obj.isMember("temperature") || !request_obj["temperature"].isDouble()
            || !request_obj.isMember("max_tokens") || !request_obj["max_tokens"].isInt()
            || !request_obj.isMember("think") || !request_obj["think"].isBool()
            || !request_obj.isMember("reasoning_effort") || !request_obj["reasoning_effort"].isString()) {
            ERR("ChatServer::ChangeModelConfig() parse request body failed");
            BuildERRResponse(res, 400, "parse request body failed");
            return;
        }
        session_id = request_obj["session_id"].asString();
        model_name = request_obj["model"].asString();
        temperature = request_obj["temperature"].asDouble();
        max_tokens = request_obj["max_tokens"].asInt();
        think = request_obj["think"].asBool();
        reasoning_effort = request_obj["reasoning_effort"].asString();
        ai_chat_sdk::Params params;
        params["temperature"] = std::to_string(temperature);
        params["max_tokens"] = std::to_string(max_tokens);
        params["think"] = think ? "true" : "false";
        params["reasoning_effort"] = reasoning_effort;
        // 更新模型参数
        if(!_chat_sdk.UpdateSessionModelConfig(user_id, session_id, params)) {
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
        INFO("ChangeModelConfig() success: {}, {}", session_id, model_name);
    }
    // 更改当前会话的模型
    // POST /api/session/model
    void ChatServer::ChangeModel(const httplib::Request& req, httplib::Response& res) {
        std::string user_id = GetUserId(req);
        if(user_id.empty()) {
            ERR("ChatServer::ChangeModel() user_id is empty");
            BuildERRResponse(res, 401, "user_id is empty");
            return;
        }
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
        if(!_chat_sdk.UpdateSessionModel(user_id, session_id, model_name)) {
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
        INFO("ChangeModel() success: {}, {}", session_id, model_name);
    }
    // 全量返回消息
    // POST /api/message
    void ChatServer::SendMessage(const httplib::Request& req, httplib::Response& res) {
        std::string user_id = GetUserId(req);
        if(user_id.empty()) {
            ERR("ChatServer::SendMessage() user_id is empty");
            BuildERRResponse(res, 401, "user_id is empty");
            return;
        }
        Json::Value request_obj;
        Json::Reader reader;
        if(!reader.parse(req.body, request_obj)) {
            ERR("ChatServer::SendMessage() parse request body failed");
            BuildERRResponse(res, 400, "parse request body failed");
            return;
        }
        // 获取会话id和消息
        std::string session_id;
        std::string message;
        if(!request_obj.isMember("session_id") || !request_obj["session_id"].isString()
            || !request_obj.isMember("message") || !request_obj["message"].isString()) {
            ERR("ChatServer::SendMessage() parse request body failed");
            BuildERRResponse(res, 400, "parse request body failed");
            return;
        }
        session_id = request_obj["session_id"].asString();
        message = request_obj["message"].asString();
        // 发送消息
        std::string ai_message = _chat_sdk.SendMessage(user_id, session_id, message);
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
        INFO("SendMessage() success: {}, {}", session_id, message);
    }
    // 流式返回消息
    // POST /api/message/async
    void ChatServer::SendMessageStream(const httplib::Request& req, httplib::Response& res) {
        std::string user_id = GetUserId(req);
        if(user_id.empty()) {
            ERR("ChatServer::SendMessageStream() user_id is empty");
            BuildERRResponse(res, 401, "user_id is empty");
            return;
        }
        Json::Value request_obj;
        Json::Reader reader;
        if(!reader.parse(req.body, request_obj)) {
            ERR("ChatServer::SendMessageStream() parse request body failed");
            BuildERRResponse(res, 400, "parse request body failed");
            return;
        }
        // 获取会话id和消息
        std::string session_id;
        std::string message;
        if(!request_obj.isMember("session_id") || !request_obj["session_id"].isString()
            || !request_obj.isMember("message") || !request_obj["message"].isString()) {
            ERR("ChatServer::SendMessageStream() parse request body failed");
            BuildERRResponse(res, 400, "parse request body failed");
            return;
        }
        session_id = request_obj["session_id"].asString();
        message = request_obj["message"].asString();
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
            [this, session_id, message, user_id](size_t offset, httplib::DataSink &data_sink)->bool{
                auto write_callback = [&](const std::string &message, bool flag) {
                    Json::Value response_obj;
                    response_obj["success"] = true;
                    response_obj["message"] = "send message success";
                    Json::Value data_obj;
                    data_obj["response"] = message;
                    data_obj["session_id"] = session_id;
                    response_obj["data"] = data_obj;
                    Json::StreamWriterBuilder builder;
                    builder["indentation"] = "";
                    std::string response_str = Json::writeString(builder, response_obj);
                    // Json::valueToQuotedString: 对chunk进行Json转换，目的防止chunk中包含一些特殊字符来破坏数据格式，
                    // 比如：在chunk中包含了两个连续的换行，就会影响SSE数据格式
                    std::string chunk = "data: " + Json::valueToQuotedString(response_str.c_str()) + "\n\n";
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
                _chat_sdk.SendMessageStream(user_id, session_id, message, write_callback);
                return true;  // 当前数据块已经发送完毕
        });
        INFO("SendMessageStream() success: {}", session_id);
    }

    // 设置路由
    void ChatServer::SetHttpRoute() {
        _server.Get("/api/sessions", 
            [this](const httplib::Request& req, httplib::Response& res) {
                GetSessionList(req, res);
        });
        _server.Get(R"(/api/session/([^/]+)/history)",
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
        _server.Delete(R"(/api/session/([^/]+))",
            [this](const httplib::Request& req, httplib::Response& res) {
                DeleteSession(req, res);
        });
        _server.Delete("/api/sessions",
            [this](const httplib::Request& req, httplib::Response& res) {
                DeleteAllSession(req, res);
        });
        _server.Post("/api/session/name",   
            [this](const httplib::Request& req, httplib::Response& res) {
                ChangeSessionName(req, res);
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
        _server.Post("/api/auth/email/code", 
            [this](const httplib::Request& req, httplib::Response& res) {
                SendEmailVerificationCode(req, res);
        });
        _server.Post("/api/auth/email/verify", 
            [this](const httplib::Request& req, httplib::Response& res) {
                VerifyEmail(req, res);
        });
        _server.Post("/api/auth/forget_password", 
            [this](const httplib::Request& req, httplib::Response& res) {
                ForgetUserPassword(req, res);
        });
        _server.Post("/api/auth/register", 
            [this](const httplib::Request& req, httplib::Response& res) {
                RegisterUser(req, res);
        });
        _server.Post("/api/auth/login", 
            [this](const httplib::Request& req, httplib::Response& res) {
                LoginUser(req, res);
        });
        _server.Post("/api/auth/logout", 
            [this](const httplib::Request& req, httplib::Response& res) {
                LogoutUser(req, res);
        });
        _server.Get("/api/auth/info", 
            [this](const httplib::Request& req, httplib::Response& res) {
                GetUserInfo(req, res);
        });
        _server.Post("/api/user/avatar", 
            [this](const httplib::Request& req, httplib::Response& res) {
                ChangeUserAvatar(req, res);
        });
        _server.Post("/api/user/name", 
            [this](const httplib::Request& req, httplib::Response& res) {
                ChangeUserName(req, res);
        });
        _server.Post("/api/user/email", 
            [this](const httplib::Request& req, httplib::Response& res) {
                ChangeUserEmail(req, res);
        });
        _server.Post("/api/user/password", 
            [this](const httplib::Request& req, httplib::Response& res) {
                ChangeUserPassword(req, res);
        });
        INFO("SetHttpRoute() success");
    }
} // namespace ai_chat_server
