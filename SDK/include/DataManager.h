#pragma once

#include <sqlite3.h>
#include "Common.h"
#include <mutex>
#include <memory>

namespace ai_chat_sdk {

    class DataManager {
    private:
        sqlite3 *_db;
        std::string _dbName;
        std::mutex _mutex;

    private:
        bool InitTable();  // 初始化数据库表
        bool SqlExec(const char *sql);  // 执行sql语句

    public:
        DataManager(std::string dbName);
        ~DataManager();

        // 会话相关操作
        // 插入会话
        bool InsertSession(Session &session);
        // 查询会话
        std::shared_ptr<Session> QuerySession(std::string session_id);
        // 更新会话时间戳
        bool UpdateSessionTime(std::string session_id);
        // 更新会话名称
        bool UpdateSessionName(std::string session_id, std::string session_name);
        // 更新会话模型
        bool UpdateSessionModel(std::string session_id, std::string model_name);
        // 删除会话
        bool DeleteSession(std::string session_id);
        // 清空所有会话
        bool ClearAllSession();
        // 查询所有会话/会话id
        std::vector<std::shared_ptr<Session>> QueryAllSession();
        std::vector<std::string> QueryAllSessionId();
        // 查询所有会话数量
        int SessionCount();


        // 消息相关操作
        // 插入消息 -> 插入消息到数据库，并更新会话时间戳
        bool InsertMessage(std::string session_id, Message &message);
        // 获取历史消息
        std::vector<Message> QueryMessage(std::string session_id);
        // 删除所有消息
        bool DeleteAllMessage(std::string session_id);
        // 查询所有消息数量
        int MessageCount(std::string session_id);
    };
}
