#include "../include/UserManager.h"
#include "../include/util/mylog.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <memory>

namespace ai_chat_sdk {
    UserManager::UserManager(std::string dbName) :_data_manager(dbName), _user_counter(0) {
        _user_counter = _data_manager.GetUserCount();
    }

    // 创建用户id
    std::string UserManager::CreateUserId() {
        // count
        // 00000001
        time_t current_time = time(nullptr);
        std::ostringstream oss;
        int count = _data_manager.GetUserCount() + 1;
        oss << std::setw(8) << std::setfill('0') << count;
        return oss.str();
    }

    std::string UserManager::GetDbName() {
        return _data_manager.GetDbName();
    }

    // 用户相关操作
    // 创建用户
    std::string UserManager::CreateUser(const std::string &user_name, 
                                        const std::string &user_avatar_path, 
                                        const std::string &email, 
                                        const std::string &password,
                                        const std::string &cookie_id) {
        _mutex.lock();
        if(_users.find(user_name) != _users.end()) {
            ERR("UserManager::CreateUser() user_name already exists: {}", user_name);
            _mutex.unlock();
            return "";
        }
        std::string user_id = CreateUserId();
        std::shared_ptr<User> user(new User(user_id, user_name, user_avatar_path, email, password, cookie_id));
        _users.emplace(user_id, user);
        _session_managers.emplace(user_id, std::make_shared<SessionManager>(GetDbName(), user_id));
        _user_ids.emplace(cookie_id, user_id);
        _mutex.unlock();
        // 数据库中创建
        if(!_data_manager.InsertUser(*user)) {
            ERR("UserManager::CreateUser() insert user failed");
            return "";
        }
        return user_id;
    }
    // 登出用户
    bool UserManager::LogoutUser(const std::string &user_id) {
        _mutex.lock();
        if(_users.find(user_id) == _users.end()) {
            ERR("UserManager::LogoutUser() user not found: {}", user_id);
            _mutex.unlock();
            return false;
        }
        _users.erase(user_id);
        _session_managers.erase(user_id);
        _user_ids.erase(user_id);
        _mutex.unlock();
        return true;
    }
    // 删除用户
    bool UserManager::DeleteUser(const std::string &user_id) {
        _mutex.lock();
        if(_users.find(user_id) == _users.end()) {
            ERR("UserManager::DeleteUser() user not found: {}", user_id);
            _mutex.unlock();
            return false;
        }
        _users.erase(user_id);
        _session_managers.erase(user_id);
        _user_ids.erase(user_id);
        _mutex.unlock();
        // 数据库中删除
        if(!_data_manager.DeleteUser(user_id)) {
            ERR("UserManager::DeleteUser() delete user from database failed");
            return false;
        }
        return true;
    }
    // 获取用户
    std::string UserManager::GetUserId(const std::string &cookie_id) {
        _mutex.lock();
        std::string user_id;
        if(_user_ids.find(cookie_id) == _user_ids.end()) {
            _mutex.unlock();
            user_id = _data_manager.QueryUserId(cookie_id);
            if(user_id.empty()) {
                ERR("UserManager::GetUserId() user not found: {}", cookie_id);
                return "";
            }
            _mutex.lock();
            _user_ids.emplace(cookie_id, user_id);
            _mutex.unlock();
            return user_id;
        }
        user_id = _user_ids[cookie_id];
        _mutex.unlock();
        return user_id;
    }
    std::shared_ptr<User> UserManager::GetUser(const std::string &user_id) {
        _mutex.lock();
        if(_users.find(user_id) == _users.end()) {
            // 内存中没有找到，在数据库中查找
            _mutex.unlock();
            auto user = _data_manager.QueryUser(user_id);
            if(user == nullptr) {
                ERR("UserManager::GetUser() user not found: {}", user_id);
                return nullptr;
            }
            _mutex.lock();
            // 加载到内存中
            _users.emplace(user_id, user);
            _session_managers.emplace(user_id, std::make_shared<SessionManager>(GetDbName(), user_id));
            _mutex.unlock();
            return user;
        }
        _mutex.unlock();
        return _users[user_id];
    }
    std::shared_ptr<User> UserManager::LoginUser(const std::string &email, const std::string &password) {
        auto user = _data_manager.LoginUser(email, password);
        if(user == nullptr) {
            ERR("UserManager::LoginUser() login user failed");
            return nullptr;
        }
        _mutex.lock();
        // 加载到内存中
        _users.emplace(user->_user_id, user);
        _session_managers.emplace(user->_user_id, std::make_shared<SessionManager>(GetDbName(), user->_user_id));
        _mutex.unlock();
        return user;
    }
    // 更新用户头像
    bool UserManager::UpdateUserAvatar(const std::string &user_id, const std::string &avatar_path, std::shared_ptr<std::string> img) {
        _mutex.lock();
        if(_users.find(user_id) == _users.end()) {
            ERR("UserManager::UpdateUserAvatar() user not found: {}", user_id);
            _mutex.unlock();
            return false;
        }
        (_users[user_id])->_user_avatar_path = avatar_path;
        std::ofstream avatar_file(avatar_path, std::ios::binary);
        if(!avatar_file.is_open()) {
            ERR("ChatServer::ChangeUserAvatar() open avatar file failed");
            _mutex.unlock();
            return false;
        }
        avatar_file.write(img->data(), img->size());
        if(!avatar_file.good()) {
            ERR("ChatServer::ChangeUserAvatar() write avatar file failed");
            _mutex.unlock();
            return false;
        }
        avatar_file.close();
        _mutex.unlock();
        // 数据库中更新
        if(!_data_manager.UpdateUserAvatar(user_id, avatar_path)) {
            ERR("UserManager::UpdateUserAvatar() update user avatar in database failed");
            return false;
        }
        return true;
    }
    // 更新用户名称
    bool UserManager::UpdateUserName(const std::string &user_id, const std::string &user_name) {
        _mutex.lock();
        if(_users.find(user_id) == _users.end()) {
            ERR("UserManager::UpdateUserName() user not found: {}", user_id);
            _mutex.unlock();
            return false;
        }
        (_users[user_id])->_user_name = user_name;
        _mutex.unlock();
        // 数据库中更新
        if(!_data_manager.UpdateUserName(user_id, user_name)) {
            ERR("UserManager::UpdateUserName() update user name in database failed");
            return false;
        }
        return true;
    }
    // 更新用户邮箱
    bool UserManager::UpdateUserEmail(const std::string &user_id, const std::string &email) {
        _mutex.lock();
        if(_users.find(user_id) == _users.end()) {
            ERR("UserManager::UpdateUserEmail() user not found: {}", user_id);
            _mutex.unlock();
            return false;
        }
        (_users[user_id])->_email = email;
        _mutex.unlock();
        // 数据库中更新
        if(!_data_manager.UpdateUserEmail(user_id, email)) {
            ERR("UserManager::UpdateUserEmail() update user email in database failed");
            return false;
        }
        return true;
    }
    // 更新用户密码
    bool UserManager::UpdateUserPassword(const std::string &user_id, const std::string &password) {
        _mutex.lock();
        if(_users.find(user_id) == _users.end()) {
            ERR("UserManager::UpdateUserPassword() user not found: {}", user_id);
            _mutex.unlock();
            return false;
        }
        (_users[user_id])->_password = password;
        _mutex.unlock();
        // 数据库中更新
        if(!_data_manager.UpdateUserPassword(user_id, password)) {
            ERR("UserManager::UpdateUserPassword() update user password in database failed");
            return false;
        }
        return true;
    }
    // 忘记密码
    bool UserManager::ForgetUserPassword(const std::string &email, const std::string &password) {
        // 数据库中更新
        if(!_data_manager.ForgetUserPassword(email, password)) {
            ERR("UserManager::ForgetUserPassword() update user password in database failed");
            return false;
        }
        // 将内存中信息删除重新登录
        _mutex.lock();
        if(_users.find(email) != _users.end()) {
            _users.erase(email);
            _session_managers.erase(email);
            _user_ids.erase(email);
        }
        _mutex.unlock();
        return true;
    }
        // 更新用户密码
    // 获取用户个数
    int64_t UserManager::GetOnlineUserCount() {
        _mutex.lock();
        int64_t count = _user_counter;
        _mutex.unlock();
        return count;
    }

    // 会话相关操作
    // 创建会话
    std::string UserManager::CreateSession(const std::string &user_id, const std::string &model_name, const std::string &session_name) {
        _mutex.lock();
        if(_session_managers.find(user_id) == _session_managers.end()) {
            ERR("UserManager::CreateSession() user not found: {}", user_id);
            _mutex.unlock();
            return "";
        }
        _mutex.unlock();
        return _session_managers[user_id]->CreateSession(model_name, session_name);
    }
    // 删除会话
    bool UserManager::DeleteSession(const std::string &user_id, const std::string &session_id) {
        _mutex.lock();
        if(_session_managers.find(user_id) == _session_managers.end()) {
            ERR("UserManager::DeleteSession() user not found: {}", user_id);
            _mutex.unlock();
            return false;
        }
        _mutex.unlock();
        return _session_managers[user_id]->DeleteSession(session_id);
    }
    // 获取会话
    std::shared_ptr<Session> UserManager::GetSession(const std::string &user_id, const std::string &session_id) {
        _mutex.lock();
        if(_session_managers.find(user_id) == _session_managers.end()) {
            ERR("UserManager::GetSession() user not found: {}", user_id);
            _mutex.unlock();
            return nullptr;
        }
        _mutex.unlock();
        return _session_managers[user_id]->GetSession(session_id);
    }
    // 获取会话所有历史信息
    std::vector<Message> UserManager::GetSessionMessages(const std::string &user_id, const std::string &session_id) {
        _mutex.lock();
        if(_session_managers.find(user_id) == _session_managers.end()) {
            ERR("UserManager::GetSessionMessages() user not found: {}", user_id);
            _mutex.unlock();
            return {};
        }
        _mutex.unlock();
        return _session_managers[user_id]->GetSessionMessages(session_id);
    }
    // 更新会话名称
    bool UserManager::UpdateSessionName(const std::string &user_id, const std::string &session_id, const std::string &session_name) {
        _mutex.lock();
        if(_session_managers.find(user_id) == _session_managers.end()) {
            ERR("UserManager::UpdateSessionName() user not found: {}", user_id);
            _mutex.unlock();
            return false;
        }
        _mutex.unlock();
        return _session_managers[user_id]->UpdateSessionName(session_id, session_name);
    }
    // 更新会话模型
    bool UserManager::UpdateSession(const std::string &user_id, const std::string &session_id, const std::string &model_name) {
        _mutex.lock();
        if(_session_managers.find(user_id) == _session_managers.end()) {
            ERR("UserManager::UpdateSession() user not found: {}", user_id);
            _mutex.unlock();
            return false;
        }
        _mutex.unlock();
        return _session_managers[user_id]->UpdateSession(session_id, model_name);
    }
    // 更新会话消息
    bool UserManager::UpdateSessionMessages(const std::string &user_id, const std::string &session_id, Message &message) {
        _mutex.lock();
        if(_session_managers.find(user_id) == _session_managers.end()) {
            ERR("UserManager::UpdateSessionMessages() user not found: {}", user_id);
            _mutex.unlock();
            return false;
        }
        _mutex.unlock();
        return _session_managers[user_id]->UpdateSessionMessages(session_id, message);
    }
    // 更新会话时间戳
    bool UserManager::UpdateSessionTimestamp(const std::string &user_id, const std::string &session_id) {
        _mutex.lock();
        if(_session_managers.find(user_id) == _session_managers.end()) {
            ERR("UserManager::UpdateSessionTimestamp() user not found: {}", user_id);
            _mutex.unlock();
            return false;
        }
        _mutex.unlock();
        return _session_managers[user_id]->UpdateSessionTimestamp(session_id);
    }
    // 获取会话列表
    std::vector<std::string> UserManager::GetSessions(const std::string &user_id) {
        _mutex.lock();
        if(_session_managers.find(user_id) == _session_managers.end()) {
            ERR("UserManager::GetSessions() user not found: {}", user_id);
            _mutex.unlock();
            return {};
        }
        _mutex.unlock();
        return _session_managers[user_id]->GetSessions();
    }
    // 获取会话数量
    int64_t UserManager::GetSessionCount(const std::string &user_id) {
        _mutex.lock();
        if(_session_managers.find(user_id) == _session_managers.end()) {
            ERR("UserManager::GetSessionCount() user not found: {}", user_id);
            _mutex.unlock();
            return 0;
        }
        _mutex.unlock();
        return _session_managers[user_id]->GetSessionCount();
    }
    // 清空所有会话
    void UserManager::ClearAllSessions(const std::string &user_id) {
        _mutex.lock();
        if(_session_managers.find(user_id) == _session_managers.end()) {
            ERR("UserManager::ClearAllSessions() user not found: {}", user_id);
            _mutex.unlock();
            return;
        }
        _mutex.unlock();
        _session_managers[user_id]->ClearAllSessions();
    }

}  // end namespace ai_chat_sdk