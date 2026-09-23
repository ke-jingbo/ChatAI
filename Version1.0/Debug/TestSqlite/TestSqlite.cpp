#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>


struct Student {
    int _id;
    std::string _name;
    std::string _sex;
    int _age;
    double _score;

    Student(int id,std::string name, std::string sex, int age, double score) 
        : _id(id), _name(name), _sex(sex), _age(age), _score(score) {}
    Student() {}
};


class StudentsDB {
private:
    sqlite3 *_db;

private:
    bool InitTable() {
        const char *sql = R"(
            create table if not exists students (
            id integer primary key autoincrement,
            name text not null,
            sex text not null,
            age integer not null,
            score real not null
            );
        )";
        int rc = sqlite3_exec(_db, sql, nullptr, nullptr, nullptr);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(_db));
            return false;
        }
        return true;
    }

public:
    StudentsDB(std::string dbName) {
        int rc = sqlite3_open(dbName.c_str(), &_db);
        if (rc) {
            fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(_db));
            sqlite3_close(_db);
            _db = nullptr;
        }
        if(!InitTable()) {
            fprintf(stderr, "Init table failed\n");
            sqlite3_close(_db);
            _db = nullptr;
        }
    }
    ~StudentsDB() { if(_db != nullptr) sqlite3_close(_db); }

    // Insert a student into the database
    bool Insert(Student &student) {
        const char *sql = R"(insert into students (name, sex, age, score) values (?, ?, ?, ?))";
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr);
        if(rc != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(_db));
            return false;
        }
        sqlite3_bind_text(stmt, 1, student._name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, student._sex.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 3, student._age);
        sqlite3_bind_double(stmt, 4, student._score);
        rc = sqlite3_step(stmt);
        if(rc != SQLITE_DONE) {
            fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(_db));
            return false;
        }
        rc = sqlite3_finalize(stmt);
        if(rc != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(_db));
            return false;
        }
        return true;
    }

    // Query all students from the database
    bool QueryAll(std::vector<Student> *students) {
        const char *sql = R"(select * from students)";
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr);
        if(rc != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(_db));
            return false;
        }
        while(sqlite3_step(stmt) == SQLITE_ROW) {
            Student s;
            s._id = sqlite3_column_int(stmt, 0);
            s._name = std::string((const char *)sqlite3_column_text(stmt, 1));
            s._sex = std::string((const char *)sqlite3_column_text(stmt, 2));
            s._age = sqlite3_column_int(stmt, 3);
            s._score = sqlite3_column_double(stmt, 4);
            students->emplace_back(s);
        }
        rc = sqlite3_finalize(stmt);
        if(rc != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(_db));
            return false;
        }
        return true;
    }

    // Update a student's score in the database
    bool UpdateScore(std::string name, double score) {
        const char *sql = R"(update students set score = ? where name = ?)";
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr);
        if(rc != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(_db));
            return false;
        }
        sqlite3_bind_double(stmt, 1, score);
        sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_TRANSIENT);
        while(sqlite3_step(stmt) == SQLITE_ROW) {
            printf("Update %s's score to %.1f\n", name.c_str(), score);
        }
        rc = sqlite3_finalize(stmt);
        if(rc != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(_db));
            return false;
        }
        return true;
    }

    // Delete a student from the database
    bool Delete(std::string name) {
        const char *sql = R"(delete from students where name = ?)";
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr);
        if(rc != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(_db));
            return false;
        }
        sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
        while(sqlite3_step(stmt) == SQLITE_ROW) {
            printf("Delete %s\n", name.c_str());
        }
        rc = sqlite3_finalize(stmt);
        if(rc != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(_db));
            return false;
        }
        return true;
    }

    // Clear all students from the database
    bool Clear() {
        const char *sql = R"(delete from students)";
        int rc = sqlite3_exec(_db, sql, nullptr, nullptr, nullptr);
        if(rc != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(_db));
            return false;
        }
        return true;
    }
};

int main() 
{
    StudentsDB db("test.db");
    db.Clear();
    Student s1(1, "张三", "男", 18, 90.0);
    Student s2(2, "李四", "女", 20, 80.0);
    Student s3(3, "王五", "男", 25, 70.0);
    db.Insert(s1);
    db.Insert(s2);
    db.Insert(s3);
    db.UpdateScore("张三", 100.0);
    db.Delete("王五");
    std::vector<Student> students;
    db.QueryAll(&students);
    for(auto &s : students) {
        printf("%d %s %s %d %.1f\n", s._id, s._name.c_str(), s._sex.c_str(), s._age, s._score);
    }

    return 0;
}