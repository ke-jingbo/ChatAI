#include "../include/SessionManager.h"
#include "../include/util/mylog.h"
#include <iostream>
#include <iomanip>

namespace ai_chat_sdk {
    SessionManager::SessionManager(std::string dbName) :_data_manager(dbName) {
        auto sessions = _data_manager.QueryAllSession();
        for(auto &session : sessions) {
            _sessions.emplace(session->_session_id, session);
        }
        _session_counter = _data_manager.SessionCount();
    }

    // 创建会话id
    std::string SessionManager::CreateSessionId() {
        // session_timestamp_count
        // session_1234567890_00000001
        time_t current_time = time(nullptr);
        _session_counter.fetch_add(1);
        std::ostringstream oss;
        oss << "session_" << current_time << "_" << std::setw(8) << std::setfill('0') << _session_counter;
        return oss.str();
    }
    // 创建消息id
    std::string SessionManager::CreateMessageId(size_t message_count) {
        // message_timestamp_count
        // message_1234567890_00000001
        time_t current_time = time(nullptr);
        std::ostringstream oss;
        message_count++;
        oss << "message_" << current_time << "_" << std::setw(8) << std::setfill('0') << message_count;
        return oss.str();
    }

    // 创建会话
    std::string SessionManager::CreateSession(const std::string &model_name) {
        // 内存中创建
        _mutex.lock();
        if(model_name.empty()) {
            ERR("SessionManager::CreateSession() model_name is empty");
            _mutex.unlock();
            return "";
        }
        std::string session_id = CreateSessionId();
        std::shared_ptr<Session> session(new Session(model_name));
        session->_session_id = session_id;
        session->_model_name = model_name;
        session->_start_time = time(nullptr);
        session->_update_time = time(nullptr);
        _sessions.emplace(session_id, session);
        _mutex.unlock();
        //  数据库中创建
        if(!_data_manager.InsertSession(*session)) {
            ERR("SessionManager::CreateSession() insert session to database failed");
            return "";
        }
        return session_id;
    }

    // 删除会话
    bool SessionManager::DeleteSession(const std::string &session_id) {
        _mutex.lock();
        if(_sessions.find(session_id) == _sessions.end()) {
            ERR("SessionManager::DeleteSession() session not found: {}", session_id);
            _mutex.unlock();
            return false;
        }
        _sessions.erase(session_id);
        _mutex.unlock();
        // 数据库中删除
        if(!_data_manager.DeleteSession(session_id)) {
            ERR("SessionManager::DeleteSession() delete session from database failed");
            return false;
        }
        return true;
    }

    // 获取会话
    std::shared_ptr<Session> SessionManager::GetSession(const std::string &session_id) {
        // 先在内存中查找会话
        _mutex.lock();
        if(_sessions.find(session_id) == _sessions.end()) {
            // 内存中没有找到，在数据库中查找
            _mutex.unlock();
            auto session = _data_manager.QuerySession(session_id);
            if(session == nullptr) {
                ERR("SessionManager::GetSession() session not found: {}", session_id);
                return nullptr;
            }
            _mutex.lock();
            // 加载到内存中
            _sessions.emplace(session_id, session);
            _mutex.unlock();
            return session;
        }
        // 加载会话的历史消息
        _mutex.unlock();
        auto messages = _data_manager.QueryMessage(session_id);
        _mutex.lock();
        _sessions[session_id]->_messages = messages;
        return _sessions[session_id];
    }

    // 获取会话所有历史信息
    std::vector<Message> SessionManager::GetSessionMessages(const std::string &session_id) {
        _mutex.lock();
        if(_sessions.find(session_id) == _sessions.end()) {
            ERR("SessionManager::GetSessionMessages() session not found: {}", session_id);
            _mutex.unlock();
            return {};
        }
        _mutex.unlock();
        return _data_manager.QueryMessage(session_id);
    }

    // 更新会话模型
    bool SessionManager::UpdateSession(const std::string &session_id, const std::string &model_name) {
        _mutex.lock();
        if(_sessions.find(session_id) == _sessions.end()) {
            ERR("SessionManager::UpdateSession() session not found: {}", session_id);
            _mutex.unlock();
            return false;
        }
        (_sessions[session_id])->_model_name = model_name;
        _mutex.unlock();
        // 数据库中更新
        if(!_data_manager.UpdateSessionModel(session_id, model_name)) {
            ERR("SessionManager::UpdateSession() update session model in database failed");
            return false;
        }
        return true;
    }

    // 添加会话消息
    bool SessionManager::UpdateSessionMessages(const std::string &session_id, Message &message) {
        _mutex.lock();
        if(_sessions.find(session_id) == _sessions.end()) {
            ERR("SessionManager::UpdateSessionMessages() session not found: {}", session_id);
            _mutex.unlock();
            return false;
        }
        // 添加消息
        (_sessions[session_id])->_messages.push_back(message);
        // 更新时间戳
        (_sessions[session_id])->_update_time = time(nullptr);
        _mutex.unlock();
        // 数据库中更新
        if(!_data_manager.InsertMessage(session_id, message)) {
            ERR("SessionManager::UpdateSessionMessages() update session messages in database failed");
            return false;
        }
        return true;
    }

    // 更新时间戳
    bool SessionManager::UpdateSessionTimestamp(const std::string &session_id) {
        _mutex.lock();
        if(_sessions.find(session_id) == _sessions.end()) {
            ERR("SessionManager::UpdateSessionTimestamp() session not found: {}", session_id);
            _mutex.unlock();
            return false;
        }
        (_sessions[session_id])->_update_time = time(nullptr);
        _mutex.unlock();
        // 数据库中更新
        if(!_data_manager.UpdateSessionTime(session_id)) {
            ERR("SessionManager::UpdateSessionTimestamp() update session timestamp in database failed");
            return false;
        }
        return true;
    }

    // 获取会话列表
    // 返回session_id而非session对象
    std::vector<std::string> SessionManager::GetSessions() {
        std::lock_guard<std::mutex> lock(_mutex);
        std::vector<std::pair<time_t, std::string>> session_list;
        for(auto &p : _sessions) session_list.push_back(std::make_pair(p.second->_update_time, p.first));
        std::sort(session_list.begin(), session_list.end(), 
            [](const std::pair<time_t, std::string> &a, const std::pair<time_t, std::string> &b) {
                return a.first > b.first;
        });
        std::vector<std::string> session_ids;
        for(auto &p : session_list) session_ids.push_back(p.second);
        assert(_data_manager.QueryAllSessionId().size() == session_ids.size());
        return session_ids;
    }

    // 获取会话数量
    int64_t SessionManager::GetSessionCount() {
        std::lock_guard<std::mutex> lock(_mutex);
        return _session_counter;
    }

    // 清空会话列表
    void SessionManager::ClearAllSessions() {
        _mutex.lock();
        _sessions.clear();
        _session_counter = 0;
        _mutex.unlock();
        _data_manager.ClearAllSession();
    }
}