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

    std::string DataManager::GetDbName() {
        return _dbName;
    }

    bool DataManager::SqlExec(const char *sql) {
        int rc = sqlite3_exec(_db, sql, nullptr, nullptr, nullptr);
        if (rc != SQLITE_OK) {
            ERR("SQL error: {}", sqlite3_errmsg(_db));
            return false;
        }
        return true;
    }

    std::string DataManager::HashPassword(const std::string& password) {
        char password_hash[crypto_pwhash_STRBYTES];
        if(crypto_pwhash_str(
            password_hash,
            password.data(),
            static_cast<unsigned long long>(password.size()),
            crypto_pwhash_OPSLIMIT_INTERACTIVE,
            crypto_pwhash_MEMLIMIT_INTERACTIVE
        ) != 0) {
            throw std::runtime_error("生成密码摘要失败");
        }
        return std::string(password_hash);
    }

    bool DataManager::InitTable() {
        if (!SqlExec("PRAGMA foreign_keys = ON;")) {
            ERR("Enable foreign keys failed");
            return false;
        }
        // 创建用户表
        std::string sql = R"(
            create table if not exists users (
            user_id text primary key,
            user_name text not null,
            user_avatar_path text not null,
            email text not null unique,
            password text not null,
            create_time integer not null,
            cookie_id text not null unique
            );
        )";
        if(!SqlExec(sql.c_str())) return false;
        sql = R"(create index if not exists login on users(email, password))";
        if(!SqlExec(sql.c_str())) return false;
        // 创建会话表
        sql = R"(
            create table if not exists sessions (
            session_id text primary key,
            session_name text not null,
            model_name text not null,
            start_time integer not null,
            update_time integer not null,
            user_id text not null,
            foreign key (user_id) references users(user_id) on delete cascade
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


    // 用户相关操作
    bool DataManager::InsertUser(User &user) {
        std::lock_guard<std::mutex> lock(_mutex);
        const char *sql = R"(insert into users values (?, ?, ?, ?, ?, ?, ?))";
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr);
        if(rc != SQLITE_OK) {
            ERR("InsertUser error: {}", sqlite3_errmsg(_db));
            return false;
        }
        sqlite3_bind_text(stmt, 1, user._user_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, user._user_name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, user._user_avatar_path.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, user._email.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 5, HashPassword(user._password).c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 6, static_cast<int64_t>(user._create_time));
        sqlite3_bind_text(stmt, 7, user._cookie_id.c_str(), -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(stmt);
        if(rc != SQLITE_DONE) {
            ERR("InsertUser error: {}", sqlite3_errmsg(_db));
            return false;
        }
        rc = sqlite3_finalize(stmt);
        return true;
    }
    std::string DataManager::QueryUserId(std::string cookie_id) {
        std::lock_guard<std::mutex> lock(_mutex);
        if(cookie_id.empty()) return "";
        const char *sql = R"(select user_id from users where cookie_id = ?)";
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr);
        if(rc != SQLITE_OK) {
            ERR("QueryUserId error: {}", sqlite3_errmsg(_db));
            sqlite3_finalize(stmt);
            return "";
        }
        sqlite3_bind_text(stmt, 1, cookie_id.c_str(), -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(stmt);
        if(rc != SQLITE_ROW) {
            ERR("QueryUserId error: {}", sqlite3_errmsg(_db));
            sqlite3_finalize(stmt);
            return "";
        }
        std::string user_id = std::string((const char *)sqlite3_column_text(stmt, 0));
        rc = sqlite3_finalize(stmt);
        return user_id;
    }
    std::shared_ptr<User> DataManager::QueryUser(std::string user_id) {
        std::lock_guard<std::mutex> lock(_mutex);
        const char *sql = R"(select user_name, user_avatar_path, email, password, create_time from users where user_id = ?)";
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr);
        if(rc != SQLITE_OK) {
            ERR("QueryUser error: {}", sqlite3_errmsg(_db));
            sqlite3_finalize(stmt);
            return nullptr;
        }
        sqlite3_bind_text(stmt, 1, user_id.c_str(), -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(stmt);
        if(rc != SQLITE_ROW) {
            ERR("QueryUser error: {}", sqlite3_errmsg(_db));
            sqlite3_finalize(stmt);
            return nullptr;
        }
        std::string user_name = std::string((const char *)sqlite3_column_text(stmt, 0));
        std::string user_avatar_path = std::string((const char *)sqlite3_column_text(stmt, 1));
        std::string email = std::string((const char *)sqlite3_column_text(stmt, 2));
        std::string password = std::string((const char *)sqlite3_column_text(stmt, 3));
        int64_t create_time = sqlite3_column_int64(stmt, 4);
        std::shared_ptr<User> user = std::make_shared<User>();
        user->_user_id = user_id;
        user->_user_name = user_name;
        user->_user_avatar_path = user_avatar_path;
        user->_email = email;
        user->_password = password;
        user->_create_time = static_cast<std::time_t>(create_time);
        sqlite3_finalize(stmt);
        return user;
    }
    std::shared_ptr<User> DataManager::LoginUser(std::string email, std::string password) {
        std::lock_guard<std::mutex> lock(_mutex);
        const char *sql = R"(select password from users where email = ?)";
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr);        
        if(rc != SQLITE_OK) {
            ERR("LoginUser error: {}", sqlite3_errmsg(_db));
            sqlite3_finalize(stmt);
            return nullptr;
        }
        sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(stmt);
        if(rc != SQLITE_ROW) {
            ERR("LoginUser error: {}", sqlite3_errmsg(_db));
            sqlite3_finalize(stmt);
            return nullptr;
        }
        std::string db_password = std::string((const char *)sqlite3_column_text(stmt, 0));
        rc = sqlite3_finalize(stmt);
        // 判断密码是否正确
        if(crypto_pwhash_str_verify(db_password.c_str(), password.c_str(), password.size()) != 0) {
            ERR("LoginUser() password is not correct");
            return nullptr;
        }
        sql = R"(select * from users where email = ?)";
        rc = sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr);
        if(rc != SQLITE_OK) {
            ERR("LoginUser error: {}", sqlite3_errmsg(_db));
            sqlite3_finalize(stmt);
            return nullptr;
        }
        sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(stmt);
        if(rc != SQLITE_ROW) {
            ERR("LoginUser error: {}", sqlite3_errmsg(_db));
            sqlite3_finalize(stmt);
            return nullptr;
        }
        std::string user_id = std::string((const char *)sqlite3_column_text(stmt, 0));
        std::shared_ptr<User> user = std::make_shared<User>();
        user->_user_id = user_id;
        user->_user_name = std::string((const char *)sqlite3_column_text(stmt, 1));
        user->_user_avatar_path = std::string((const char *)sqlite3_column_text(stmt, 2));
        user->_email = email;
        user->_password = password;
        user->_create_time = static_cast<std::time_t>(sqlite3_column_int64(stmt, 5));
        user->_cookie_id = std::string((const char *)sqlite3_column_text(stmt, 6));
        sqlite3_finalize(stmt);
        return user;
    }
    bool DataManager::UpdateUserAvatar(std::string user_id, std::string avatar_path) {
        std::lock_guard<std::mutex> lock(_mutex);
        const char *sql = R"(update users set user_avatar_path = ? where user_id = ?)";
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr);
        if(rc != SQLITE_OK) {
            ERR("UpdateUserAvatar error: {}", sqlite3_errmsg(_db));
            return false;
        }
        sqlite3_bind_text(stmt, 1, avatar_path.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, user_id.c_str(), -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(stmt);
        if(rc != SQLITE_DONE) {
            ERR("UpdateUserAvatar error: {}", sqlite3_errmsg(_db));
            return false;
        }
        rc = sqlite3_finalize(stmt);
        return true;
    }
    bool DataManager::UpdateUserName(std::string user_id, std::string user_name) {
        std::lock_guard<std::mutex> lock(_mutex);
        const char *sql = R"(update users set user_name = ? where user_id = ?)";
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr);
        if(rc != SQLITE_OK) {
            ERR("UpdateUserName error: {}", sqlite3_errmsg(_db));
            return false;
        }
        sqlite3_bind_text(stmt, 1, user_name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, user_id.c_str(), -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(stmt);
        if(rc != SQLITE_DONE) {
            ERR("UpdateUserName error: {}", sqlite3_errmsg(_db));
            return false;
        }
        rc = sqlite3_finalize(stmt);
        return true;
    }
    bool DataManager::UpdateUserEmail(std::string user_id, std::string email) {
        std::lock_guard<std::mutex> lock(_mutex);
        const char *sql = R"(update users set email = ? where user_id = ?)";
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr);
        if(rc != SQLITE_OK) {
            ERR("UpdateUserEmail error: {}", sqlite3_errmsg(_db));
            return false;
        }
        sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, user_id.c_str(), -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(stmt);
        if(rc != SQLITE_DONE) {
            ERR("UpdateUserEmail error: {}", sqlite3_errmsg(_db));
            return false;
        }
        rc = sqlite3_finalize(stmt);
        return true;
    }
    bool DataManager::UpdateUserPassword(std::string user_id, std::string password) {
        std::lock_guard<std::mutex> lock(_mutex);
        const char *sql = R"(update users set password = ? where user_id = ?)";
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr);
        if(rc != SQLITE_OK) {
            ERR("UpdateUserPassword error: {}", sqlite3_errmsg(_db));
            return false;
        }
        sqlite3_bind_text(stmt, 1, HashPassword(password).c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, user_id.c_str(), -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(stmt);
        if(rc != SQLITE_DONE) {
            ERR("UpdateUserPassword error: {}", sqlite3_errmsg(_db));
            return false;
        }
        rc = sqlite3_finalize(stmt);
        return true;
    }
    bool DataManager::ForgetUserPassword(std::string email, std::string password) {
        std::lock_guard<std::mutex> lock(_mutex);
        const char *sql = R"(update users set password = ? where email = ?)";
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr);
        if(rc != SQLITE_OK) {
            ERR("ForgetUserPassword error: {}", sqlite3_errmsg(_db));
            return false;
        }
        sqlite3_bind_text(stmt, 1, HashPassword(password).c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, email.c_str(), -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(stmt);
        if(rc != SQLITE_DONE) {
            ERR("ForgetUserPassword error: {}", sqlite3_errmsg(_db));
            return false;
        }
        rc = sqlite3_finalize(stmt);
        return true;
    }
    int64_t DataManager::GetUserCount() {
        std::lock_guard<std::mutex> lock(_mutex);
        const char *sql = R"(select count(*) from users)";
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr);
        if(rc != SQLITE_OK) {
            ERR("GetUserCount error: {}", sqlite3_errmsg(_db));
            sqlite3_finalize(stmt);
            return 0;
        }
        int count = 0;
        rc = sqlite3_step(stmt);
        if(rc != SQLITE_DONE && rc != SQLITE_ROW) {
            ERR("GetUserCount error: {}", sqlite3_errmsg(_db));
            sqlite3_finalize(stmt);
            return 0;
        }
        count = sqlite3_column_int(stmt, 0);
        rc = sqlite3_finalize(stmt);
        return count;
    }
    bool DataManager::DeleteUser(std::string user_id) {
        std::lock_guard<std::mutex> lock(_mutex);
        const char *sql = R"(delete from users where user_id = ?)";
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr);
        if(rc != SQLITE_OK) {
            ERR("DeleteUser error: {}", sqlite3_errmsg(_db));
            return false;
        }
        sqlite3_bind_text(stmt, 1, user_id.c_str(), -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(stmt);
        if(rc != SQLITE_DONE) {
            ERR("DeleteUser error: {}", sqlite3_errmsg(_db));
            return false;
        }
        rc = sqlite3_finalize(stmt);
        return true;
    }


    // 会话相关操作
    bool DataManager::InsertSession(Session &session, const std::string &user_id) {
        std::lock_guard<std::mutex> lock(_mutex);
        const char *sql = R"(insert into sessions values (?, ?, ?, ?, ?, ?))";
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr);
        if(rc != SQLITE_OK) {
            ERR("InsertSession error: {}", sqlite3_errmsg(_db));
            return false;
        }
        sqlite3_bind_text(stmt, 1, session._session_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, session._session_name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, session._model_name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 4, static_cast<int64_t>(session._start_time));
        sqlite3_bind_int64(stmt, 5, static_cast<int64_t>(session._update_time));
        sqlite3_bind_text(stmt, 6, user_id.c_str(), -1, SQLITE_TRANSIENT);
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
        const char *sql = R"(select session_name, model_name, start_time, update_time from sessions where session_id = ?)";
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
        std::string session_name = std::string((const char *)sqlite3_column_text(stmt, 0));
        std::string model_name = std::string((const char *)sqlite3_column_text(stmt, 1));
        int64_t start_time = sqlite3_column_int64(stmt, 2);
        int64_t update_time = sqlite3_column_int64(stmt, 3);
        std::shared_ptr<Session> session = std::make_shared<Session>(model_name);
        session->_session_id = session_id;
        session->_start_time = static_cast<std::time_t>(start_time);
        session->_update_time = static_cast<std::time_t>(update_time);
        sqlite3_finalize(stmt);
        return session;
    }

    bool DataManager::UpdateSessionName(std::string session_id, std::string session_name) {
        std::lock_guard<std::mutex> lock(_mutex);
        const char *sql = R"(update sessions set session_name = ? where session_id = ?)";
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr);
        if(rc != SQLITE_OK) {
            ERR("UpdateSessionName error: {}", sqlite3_errmsg(_db));
            return false;
        }
        sqlite3_bind_text(stmt, 1, session_name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, session_id.c_str(), -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(stmt);
        if(rc != SQLITE_DONE) {
            ERR("UpdateSessionName error: {}", sqlite3_errmsg(_db));
            return false;
        }
        rc = sqlite3_finalize(stmt);
        return true;
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

    bool DataManager::ClearAllSession(const std::string &user_id) {
        std::lock_guard<std::mutex> lock(_mutex);
        const char *sql = R"(delete from sessions where user_id = ?)";
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr);
        if(rc != SQLITE_OK) {
            ERR("ClearAllSession error: {}", sqlite3_errmsg(_db));
            sqlite3_finalize(stmt);
            return false;
        }
        sqlite3_bind_text(stmt, 1, user_id.c_str(), -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(stmt);
        if(rc != SQLITE_DONE) {
            ERR("ClearAllSession error: {}", sqlite3_errmsg(_db));
            sqlite3_finalize(stmt);
            return false;
        }
        rc = sqlite3_finalize(stmt);
        return true;
    }

    std::vector<std::string> DataManager::QueryAllSessionId(const std::string &user_id) {
        std::lock_guard<std::mutex> lock(_mutex);
        const char *sql = R"(select session_id from sessions where user_id = ? order by update_time desc)";
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr);
        if(rc != SQLITE_OK) {
            ERR("QueryAllSessionId error: {}", sqlite3_errmsg(_db));
            sqlite3_finalize(stmt);
            return std::vector<std::string>();
        }
        sqlite3_bind_text(stmt, 1, user_id.c_str(), -1, SQLITE_TRANSIENT);
        std::vector<std::string> session_ids;
        while(sqlite3_step(stmt) == SQLITE_ROW)
            session_ids.emplace_back(std::string((const char *)sqlite3_column_text(stmt, 0)));
        rc = sqlite3_finalize(stmt);
        return session_ids;
    }

    std::vector<std::shared_ptr<Session>> DataManager::QueryAllSession(const std::string &user_id) {
        std::lock_guard<std::mutex> lock(_mutex);
        const char *sql = R"(select * from sessions where user_id = ? order by update_time desc)";
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr);
        if(rc != SQLITE_OK) {
            ERR("QueryAllSession error: {}", sqlite3_errmsg(_db));
            sqlite3_finalize(stmt);
            return std::vector<std::shared_ptr<Session>>();
        }
        sqlite3_bind_text(stmt, 1, user_id.c_str(), -1, SQLITE_TRANSIENT);
        std::vector<std::shared_ptr<Session>> sessions;
        while(sqlite3_step(stmt) == SQLITE_ROW) {
            std::shared_ptr<Session> session = std::make_shared<Session>();
            session->_session_id = std::string((const char *)sqlite3_column_text(stmt, 0));
            session->_session_name = std::string((const char *)sqlite3_column_text(stmt, 1));
            session->_model_name = std::string((const char *)sqlite3_column_text(stmt, 2));
            session->_start_time = sqlite3_column_int64(stmt, 3);
            session->_update_time = sqlite3_column_int64(stmt, 4);
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
