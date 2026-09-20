#include "../include/DataManager.h"
#include "../include/util/mylog.h"

namespace ai_chat_sdk {
    DataManager::DataManager(std::string dbName) :_dbName(dbName), _db(nullptr) {
        int rc = sqlite3_open(dbName.c_str(), &_db);
        if (rc != SQLITE_OK) {
            ERR("Can't open database: {}", sqlite3_errmsg(_db));
            sqlite3_close(_db);
            _db = nullptr;
        }
        if(!InitTable()) {
            ERR("Init table failed");
            sqlite3_close(_db);
            _db = nullptr;
        }
    }

    DataManager::~DataManager() {
        if(_db != nullptr) {
            sqlite3_close(_db);
            _db = nullptr;
        }
    }

    bool DataManager::SqlExec(const char *sql) {
        int rc = sqlite3_exec(_db, sql, nullptr, nullptr, nullptr);
        if (rc != SQLITE_OK) {
            ERR("SQL error: {}", sqlite3_errmsg(_db));
            return false;
        }
        return true;
    }

    bool DataManager::InitTable() {
        // 创建会话表
        std::string sql = R"(
            create table if not exists sessions (
            session_id text primary key,
            model_name text not null,
            start_time integer not null,
            update_time integer not null
            );
        )";
        if(!SqlExec(sql.c_str())) return false;
        // 创建消息表
        sql = R"(
            create table if not exists messages (
            message_id text primary key,
            role text not null,
            content text not null,
            msg_timestamp integer not null,
            session_id text not null,
            foreign key (session_id) references sessions(session_id) on delete cascade
            );
        )";
        if(!SqlExec(sql.c_str())) return false;
        return true;
    }


    // 会话相关操作
    bool DataManager::InsertSession(Session &session) {
        std::lock_guard<std::mutex> lock(_mutex);
        const char *sql = R"(insert into sessions values (?, ?, ?, ?))";
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr);
        if(rc != SQLITE_OK) {
            ERR("InsertSession error: {}", sqlite3_errmsg(_db));
            return false;
        }
        sqlite3_bind_text(stmt, 1, session._session_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, session._model_name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 3, static_cast<int64_t>(session._start_time));
        sqlite3_bind_int64(stmt, 4, static_cast<int64_t>(session._update_time));
        rc = sqlite3_step(stmt);
        if(rc != SQLITE_DONE) {
            ERR("InsertSession error: {}", sqlite3_errmsg(_db));
            return false;
        }
        rc = sqlite3_finalize(stmt);
        return true;
    }

    std::shared_ptr<Session> DataManager::QuerySession(std::string session_id) {
        std::lock_guard<std::mutex> lock(_mutex);
        const char *sql = R"(select model_name, start_time, update_time from sessions where session_id = ?)";
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr);
        if(rc != SQLITE_OK) {
            ERR("QuerySession error: {}", sqlite3_errmsg(_db));
            sqlite3_finalize(stmt);
            return nullptr;
        }
        sqlite3_bind_text(stmt, 1, session_id.c_str(), -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(stmt);
        if(rc != SQLITE_ROW) {
            ERR("QuerySession error: {}", sqlite3_errmsg(_db));
            sqlite3_finalize(stmt);
            return nullptr;
        }
        std::string model_name = std::string((const char *)sqlite3_column_text(stmt, 0));
        int64_t start_time = sqlite3_column_int64(stmt, 1);
        int64_t update_time = sqlite3_column_int64(stmt, 2);
        std::shared_ptr<Session> session = std::make_shared<Session>(model_name);
        session->_session_id = session_id;
        session->_start_time = static_cast<std::time_t>(start_time);
        session->_update_time = static_cast<std::time_t>(update_time);
        sqlite3_finalize(stmt);
        return session;
    }

    bool DataManager::UpdateSessionTime(std::string session_id) {
        std::lock_guard<std::mutex> lock(_mutex);
        const char *sql = R"(update sessions set update_time = ? where session_id = ?)";
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr);
        if(rc != SQLITE_OK) {
            ERR("UpdateSessionTime error: {}", sqlite3_errmsg(_db));
            return false;
        }
        sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(std::time(nullptr)));
        sqlite3_bind_text(stmt, 2, session_id.c_str(), -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(stmt);
        if(rc != SQLITE_DONE) {
            ERR("UpdateSessionTime error: {}", sqlite3_errmsg(_db));
            return false;
        }
        rc = sqlite3_finalize(stmt);
        if(rc != SQLITE_OK) {
            ERR("UpdateSessionTime error: {}", sqlite3_errmsg(_db));
            return false;
        }
        return true;
    }

    bool DataManager::UpdateSessionModel(std::string session_id, std::string model_name) {
        std::lock_guard<std::mutex> lock(_mutex);
        const char *sql = R"(update sessions set model_name = ? where session_id = ?)";
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr);
        if(rc != SQLITE_OK) {
            ERR("UpdateSessionModel error: {}", sqlite3_errmsg(_db));
            return false;
        }
        sqlite3_bind_text(stmt, 1, model_name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, session_id.c_str(), -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(stmt);
        if(rc != SQLITE_DONE) {
            ERR("UpdateSessionModel error: {}", sqlite3_errmsg(_db));
            return false;
        }
        rc = sqlite3_finalize(stmt);
        return true;
    }

    bool DataManager::DeleteSession(std::string session_id) {
        std::lock_guard<std::mutex> lock(_mutex);
        const char *sql = R"(delete from sessions where session_id = ?)";
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr);
        if(rc != SQLITE_OK) {
            ERR("DeleteSession error: {}", sqlite3_errmsg(_db));
            return false;
        }
        sqlite3_bind_text(stmt, 1, session_id.c_str(), -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(stmt);
        if(rc != SQLITE_DONE) {
            ERR("DeleteSession error: {}", sqlite3_errmsg(_db));
            return false;
        }
        rc = sqlite3_finalize(stmt);
        return true;
    }

    bool DataManager::ClearAllSession() {
        std::lock_guard<std::mutex> lock(_mutex);
        const char *sql = R"(delete from sessions)";
        if(!SqlExec(sql)) return false;
        return true;
    }

    std::vector<std::string> DataManager::QueryAllSessionId() {
        std::lock_guard<std::mutex> lock(_mutex);
        const char *sql = R"(select session_id from sessions order by update_time desc)";
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr);
        if(rc != SQLITE_OK) {
            ERR("QueryAllSessionId error: {}", sqlite3_errmsg(_db));
            sqlite3_finalize(stmt);
            return std::vector<std::string>();
        }
        std::vector<std::string> session_ids;
        while(sqlite3_step(stmt) == SQLITE_ROW)
            session_ids.emplace_back(std::string((const char *)sqlite3_column_text(stmt, 0)));
        rc = sqlite3_finalize(stmt);
        return session_ids;
    }

    std::vector<std::shared_ptr<Session>> DataManager::QueryAllSession() {
        std::lock_guard<std::mutex> lock(_mutex);
        const char *sql = R"(select * from sessions order by update_time desc)";
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr);
        if(rc != SQLITE_OK) {
            ERR("QueryAllSession error: {}", sqlite3_errmsg(_db));
            sqlite3_finalize(stmt);
            return std::vector<std::shared_ptr<Session>>();
        }
        std::vector<std::shared_ptr<Session>> sessions;
        while(sqlite3_step(stmt) == SQLITE_ROW) {
            std::shared_ptr<Session> session = std::make_shared<Session>();
            session->_session_id = std::string((const char *)sqlite3_column_text(stmt, 0));
            session->_model_name = std::string((const char *)sqlite3_column_text(stmt, 1));
            session->_start_time = sqlite3_column_int64(stmt, 2);
            session->_update_time = sqlite3_column_int64(stmt, 3);
            sessions.emplace_back(session);
        }
        rc = sqlite3_finalize(stmt);
        return sessions;
    }

    int DataManager::SessionCount() {
        std::lock_guard<std::mutex> lock(_mutex);
        const char *sql = R"(select count(*) from sessions)";
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr);
        if(rc != SQLITE_OK) {
            ERR("SessionCount error: {}", sqlite3_errmsg(_db));
            sqlite3_finalize(stmt);
            return 0;
        }
        int count = 0;
        rc = sqlite3_step(stmt);
        if(rc != SQLITE_DONE && rc != SQLITE_ROW) {
            ERR("SessionCount error: {}", sqlite3_errmsg(_db));
            sqlite3_finalize(stmt);
            return 0;
        }
        count = sqlite3_column_int(stmt, 0);
        rc = sqlite3_finalize(stmt);
        return count;
    }


    // 消息相关操作
    bool DataManager::InsertMessage(std::string session_id, Message &message) {
        std::lock_guard<std::mutex> lock(_mutex);
        const char *sql = R"(insert into messages values (?, ?, ?, ?, ?))";
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr);
        if(rc != SQLITE_OK) {
            ERR("InsertMessage error: {}", sqlite3_errmsg(_db));
            return false;
        }
        sqlite3_bind_text(stmt, 1, message._messageid.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, message._role.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, message._content.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 4, static_cast<int64_t>(message._timestamp));
        sqlite3_bind_text(stmt, 5, session_id.c_str(), -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(stmt);
        if(rc != SQLITE_DONE) {
            ERR("InsertMessage error: {}", sqlite3_errmsg(_db));
            return false;
        }
        rc = sqlite3_finalize(stmt);
        return true;
    }

    std::vector<Message> DataManager::QueryMessage(std::string session_id) {
        std::lock_guard<std::mutex> lock(_mutex);
        const char *sql = R"(select role, content, msg_timestamp, message_id from messages where session_id = ? order by msg_timestamp desc)";
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr);
        if(rc != SQLITE_OK) {
            ERR("QueryMessage error: {}", sqlite3_errmsg(_db));
            sqlite3_finalize(stmt);
            return std::vector<Message>();
        }
        sqlite3_bind_text(stmt, 1, session_id.c_str(), -1, SQLITE_TRANSIENT);
        std::vector<Message> messages;
        while(sqlite3_step(stmt) == SQLITE_ROW) {
            Message message;
            message._role = std::string((const char *)sqlite3_column_text(stmt, 0));
            message._content = std::string((const char *)sqlite3_column_text(stmt, 1));
            message._timestamp = sqlite3_column_int64(stmt, 2);
            message._messageid = std::string((const char *)sqlite3_column_text(stmt, 3));
            messages.emplace_back(message);
        }
        rc = sqlite3_finalize(stmt);
        return messages;
    }

    bool DataManager::DeleteAllMessage(std::string session_id) {
        std::lock_guard<std::mutex> lock(_mutex);
        const char *sql = R"(delete from messages where session_id = ?)";
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr);
        if(rc != SQLITE_OK) {
            ERR("DeleteAllMessage error: {}", sqlite3_errmsg(_db));
            return false;
        }
        sqlite3_bind_text(stmt, 1, session_id.c_str(), -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(stmt);
        if(rc != SQLITE_DONE) {
            ERR("DeleteAllMessage error: {}", sqlite3_errmsg(_db));
            return false;
        }
        rc = sqlite3_finalize(stmt);
        return true;
    }

    int DataManager::MessageCount(std::string session_id) {
        std::lock_guard<std::mutex> lock(_mutex);
        const char *sql = R"(select count(*) from messages where session_id = ?)";
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr);
        if(rc != SQLITE_OK) {
            ERR("MessageCount error: {}", sqlite3_errmsg(_db));
            sqlite3_finalize(stmt);
            return 0;
        }
        sqlite3_bind_text(stmt, 1, session_id.c_str(), -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(stmt);
        if(rc != SQLITE_DONE && rc != SQLITE_ROW) {
            ERR("MessageCount error: {}", sqlite3_errmsg(_db));
            sqlite3_finalize(stmt);
            return 0;
        }
        int count = sqlite3_column_int(stmt, 0);
        rc = sqlite3_finalize(stmt);
        return count;
    }

}  // end namespace ai_chat_sdk