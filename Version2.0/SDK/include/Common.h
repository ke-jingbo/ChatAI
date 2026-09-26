#pragma once

#include <array>
#include <string>
#include <vector>
#include <ctime>
#include <map>
#include <functional>
#include <memory>
#include <atomic>
#include <iomanip>
#include <stdexcept>

#include <openssl/rand.h>

namespace ai_chat_sdk {
    struct Message;
    class SessionManager;
    // 会话
    struct Session {
        std::string _session_id;        // 会话的id
        std::string _session_name = "new session";      // 会话的名称
        std::string _model_name;        // 会话的模型名称
        std::time_t _start_time;        // 会话的开始时间
        std::time_t _update_time;       // 会话的更新时间
        std::vector<Message> _messages; // 会话的消息列表

        Session(const std::string modle_name = "", const std::string session_name = "new session") 
            :_model_name(modle_name), _session_name(session_name) {}
    };
    // 消息
    struct Message {
        std::string _role;          // 消息的角色
        std::string _content;       // 消息的内容
        std::time_t _timestamp;     // 消息的时间戳
        std::string _messageid;     // 消息的id
        static std::atomic<int64_t> _message_counter; // 消息计数器

        static std::string CreateMessageId() {
            // message_timestamp_count
            // message_1234567890_00000001
            time_t current_time = time(nullptr);
            std::ostringstream oss;
            int count = _message_counter.fetch_add(1);
            oss << "message_" << current_time << "_" << std::setw(8) << std::setfill('0') << count;
            return oss.str();
        }

        Message(const std::string role = "", const std::string content = "") 
            :_role(role)
            ,_content(content)
            ,_timestamp(time(nullptr)) 
            ,_messageid(CreateMessageId()) {}
    };
    inline std::atomic<int64_t> Message::_message_counter = {0};

    
    // 模型配置
    struct ModelConfig {
        double _temperature = 0.8;              // 模型的温度值
        int _max_tokens =2048;                  // 模型的最大token数
        bool _think = true;                     // 模型的是否考虑思考
        std::string _reasoning_effort = "high"; // 模型的思考程度
        // 构造函数
        ModelConfig(double temperature = 0.8, 
                    int max_tokens = 2048, 
                    bool think = true, 
                    std::string reasoning_effort = "high")
            :_temperature(temperature)
            ,_max_tokens(max_tokens)
            ,_think(think)            
            ,_reasoning_effort(reasoning_effort) {}
    };
    // 模型
    struct Model {
        std::string _name;      // 模型的名称
        std::string _desc;      // 模型的描述
        std::string _provider;  // 模型的提供者
        ModelConfig _config;    // 模型的配置
        bool _is_active;        // 是否激活
        // 构造函数
        Model(const std::string name = "", 
            const std::string provider = "",
            const std::string desc = "",
            ModelConfig config = ModelConfig())
            :_name(name)
            ,_desc(desc)
            ,_provider(provider)
            ,_is_active(false)
            ,_config(config) {}
    };


    // 云端接入模型配置
    struct ProviderConfig{
        std::string _provider;  // 模型的提供者
        ProviderConfig(const std::string provider = "") :_provider(provider) {}
        virtual ~ProviderConfig() = default;  // 用于继承类向下的类型转换
    };
    struct APIConfig : public ProviderConfig{
        std::string _api_key;   // 模型的apikey
        APIConfig(const std::string provider_name, const std::string api_key = "") 
            :_api_key(api_key), ProviderConfig(provider_name) {}
    };
    // 本地模型配置
    struct LocalConfig : public ProviderConfig {
        std::string _name;      // 模型的名称
        std::string _desc;      // 模型的描述
        std::string _path;      // 模型的路径 baseurl
        LocalConfig(const std::string provider_name, const std::string name, const std::string path, const std::string desc = "") 
            :_name(name), _desc(desc), _path(path), ProviderConfig(provider_name) {}
    };


    using Messages = std::vector<Message>;
    using Params = std::map<std::string, std::string>;
    using StreamCallback = std::function<void(const std::string &message, bool flag)>;  
    // 处理流式信息的回调函数 第一个参数表示消息内容，第二个参数表示是否是最后一条消息

    // 用户管理
    struct User {
        std::string _user_id;               // 用户的id
        std::string _user_name;             // 用户的名称
        std::string _user_avatar_path;      // 用户的头像路径
        std::string _email;                 // 用户的邮箱
        std::string _password;              // 用户的密码
        time_t _create_time;                // 用户的创建时间
        std::string _cookie_id;             // 用户的cookie id

        User(const std::string user_id = "", 
            const std::string user_name = "", 
            const std::string user_avatar_path = "", 
            const std::string email = "", 
            const std::string password = "", 
            const std::string cookie_id = "")
            :_user_id(user_id)
            ,_user_name(user_name)
            ,_user_avatar_path(user_avatar_path)
            ,_email(email)
            ,_password(password)
            ,_create_time(time(nullptr))
            ,_cookie_id(cookie_id) {}
    };
}  // end namespace ai_chat_sdk
