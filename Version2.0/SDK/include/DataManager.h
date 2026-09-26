#pragma once

#include <sqlite3.h>
#include "Common.h"
#include <mutex>
#include <memory>
#include <sodium.h>

namespace ai_chat_sdk {

    class DataManager {
    private:
        sqlite3 *_db;
        std::string _dbName;
        std::mutex _mutex;

    private:
        bool InitTable();               // 初始化数据库表
        bool SqlExec(const char *sql);  // 执行sql语句
        std::string HashPassword(const std::string& password);

    public:
        DataManager(std::string dbName);
        ~DataManager();

        std::string GetDbName();

        // 用户相关操作
        // 插入用户
        bool InsertUser(User &user);
        // 查询用户
        std::string QueryUserId(std::string cookie_id);
        std::shared_ptr<User> QueryUser(std::string user_id);
        std::shared_ptr<User> LoginUser(std::string email, std::string password);
        // 更新用户头像
        bool UpdateUserAvatar(std::string user_id, std::string avatar_path);
        // 更新用户名称
        bool UpdateUserName(std::string user_id, std::string user_name);
        // 更新用户邮箱
        bool UpdateUserEmail(std::string user_id, std::string email);
        // 更新用户密码
        bool UpdateUserPassword(std::string user_id, std::string password);
        // 忘记密码
        bool ForgetUserPassword(std::string email, std::string password);
        // 获取用户个数
        int64_t GetUserCount();
        // 删除用户
        bool DeleteUser(std::string user_id);

        // 会话相关操作
        // 插入会话
        bool InsertSession(Session &session, const std::string &user_id);
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
        bool ClearAllSession(const std::string &user_id);
        // 查询所有会话/会话id
        std::vector<std::shared_ptr<Session>> QueryAllSession(const std::string &user_id);
        std::vector<std::string> QueryAllSessionId(const std::string &user_id);
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
