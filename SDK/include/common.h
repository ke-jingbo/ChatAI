#pragma once

#include <string>
#include <vector>
#include <ctime>

namespace ai_chat_sdk {

// 消息
struct Message {
    std::string _role;          // 消息的角色
    std::string _messageid;     // 消息的id
    std::string _content;       // 消息的内容
    std::time_t _timestamp;     // 消息的时间戳

    Message(const std::string role = "", const std::string content = "")
        :_role(role)
        ,_content(content)
        ,_timestamp(0) {}
};


// 模型
struct Model {
    std::string _name;      // 模型的名称
    std::string _desc;      // 模型的描述
    std::string _path;      // 模型的路径 baseurl
    std::string _provider;  // 模型的提供者
    bool _is_active;        // 是否激活

    Model(const std::string name = "", 
        const std::string desc = "", 
        const std::string path = "", 
        const std::string provider = "")
        :_name(name)
        ,_desc(desc)
        ,_path(path)
        ,_provider(provider)
        ,_is_active(false) {}
};

// 模型api配置
struct APIModelConfig {
    std::string _api_key;   // 模型的apikey
};

// 模型配置
struct ModelConfig {
    std::string _name;              // 模型的名称
    double _temperature = 0.8;      // 模型的温度值
    int _max_tokens =2048;          // 模型的最大token数
};

// 会话
struct Session {
    std::string _session_id;        // 会话的id
    std::string _model_name;        // 会话的模型名称
    std::time_t _start_time;        // 会话的开始时间
    std::time_t _update_time;       // 会话的更新时间
    std::vector<Message> _messages; // 会话的消息列表

    Session(const std::string modle_name = "") :_model_name(modle_name) {}
};

}  // namespace ai_chat_sdk