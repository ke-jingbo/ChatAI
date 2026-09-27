#pragma once

#include "Common.h"
#include "DataManager.h"
#include "SessionManager.h"

namespace ai_chat_sdk {
    class UserManager {
    private:
        std::map<std::string, std::shared_ptr<SessionManager>> _session_managers;
        std::map<std::string, std::shared_ptr<User>> _users;
        std::map<std::string, std::string> _user_ids;
        DataManager _data_manager;
        std::atomic<uint64_t> _user_counter = {0};  // 用户计数器
        std::mutex _mutex;

    private:
        std::string CreateUserId();
        std::string GetDbName();

    public:
        UserManager(std::string dbName);
        // 用户相关操作
        // 创建用户
        std::string CreateUser(const std::string &user_name, 
                            const std::string &user_avatar_path, 
                            const std::string &email, 
                            const std::string &password,
                            const std::string &cookie_id);
        // 登出用户
        bool LogoutUser(const std::string &user_id);
        // 删除用户
        bool DeleteUser(const std::string &user_id);
        // 获取用户
        std::string GetUserId(const std::string &cookie_id);
        std::shared_ptr<User> GetUser(const std::string &user_id);
        std::shared_ptr<User> LoginUser(const std::string &email, const std::string &password);
        // 更新用户头像
        bool UpdateUserAvatar(const std::string &user_id, const std::string &avatar_path, std::shared_ptr<std::string> img);
        // 更新用户名称
        bool UpdateUserName(const std::string &user_id, const std::string &user_name);
        // 更新用户邮箱
        bool UpdateUserEmail(const std::string &user_id, const std::string &email);
        // 更新用户密码
        bool UpdateUserPassword(const std::string &user_id, const std::string &password);
        // 忘记密码
        bool ForgetUserPassword(const std::string &email, const std::string &password);
        // 获取用户个数
        int64_t GetOnlineUserCount();

        // 会话相关操作
        // 创建会话
        std::string CreateSession(const std::string &user_id, const std::string &model_name, const std::string &session_name);
        // 删除会话
        bool DeleteSession(const std::string &user_id, const std::string &session_id);
        // 获取会话
        std::shared_ptr<Session> GetSession(const std::string &user_id, const std::string &session_id);
        // 获取会话所有历史信息
        std::vector<Message> GetSessionMessages(const std::string &user_id, const std::string &session_id);
        // 更新会话名称
        bool UpdateSessionName(const std::string &user_id, const std::string &session_id, const std::string &session_name);
        // 更新会话模型
        bool UpdateSession(const std::string &user_id, const std::string &session_id, const std::string &model_name);
        // 更新会话消息
        bool UpdateSessionMessages(const std::string &user_id, const std::string &session_id, Message &message);
        // 更新会话时间戳
        bool UpdateSessionTimestamp(const std::string &user_id, const std::string &session_id);
        // 获取会话列表
        std::vector<std::string> GetSessions(const std::string &user_id);
        // 获取会话数量
        int64_t GetSessionCount(const std::string &user_id);
        // 清空所有会话
        void ClearAllSessions(const std::string &user_id);
    };
}