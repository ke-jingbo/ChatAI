#pragma once

#include "Common.h"
#include <memory>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include "DataManager.h"

namespace ai_chat_sdk {

    class SessionManager {
    private:
        std::unordered_map<std::string, std::shared_ptr<Session>> _sessions;  // 会话列表
        std::mutex _mutex;                                                    // 会话列表锁              
        std::atomic<int64_t> _session_counter = {0};                       // 会话计数器
        DataManager _data_manager;                                           // 数据库管理器

    private:
        // 创建会话id
        std::string CreateSessionId();
        // 创建消息id
        std::string CreateMessageId(size_t message_count);

    public:
        SessionManager(std::string dbName);
        // 创建会话
        std::string CreateSession(const std::string &model_name);
        // 删除会话
        bool DeleteSession(const std::string &session_id);
        // 获取会话
        std::shared_ptr<Session> GetSession(const std::string &session_id);
        // 获取会话所有历史信息
        std::vector<Message> GetSessionMessages(const std::string &session_id);
        // 更新会话模型
        bool UpdateSession(const std::string &session_id, const std::string &model_name);
        // 更新会话消息
        bool UpdateSessionMessages(const std::string &session_id, Message &message);
        // 更新时间戳
        bool UpdateSessionTimestamp(const std::string &session_id);
        // 获取会话列表
        std::vector<std::string> GetSessions();
        // 获取会话数量
        int64_t GetSessionCount();
        // 清空会话列表
        void ClearAllSessions();
    };
}