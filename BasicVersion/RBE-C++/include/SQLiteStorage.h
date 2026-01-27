#pragma once
#include <Storage.h>
#include <my_utils.h>
#include <sqlite3.h>

#include <iostream>
// ---------------------------------------------------------
// "SQLite版" 的实现。
// ---------------------------------------------------------
class SQLiteStorage : public Storage {
    sqlite3* db;

    // 辅助：执行无返回值的 SQL (如 CREATE TABLE)
    void exec_sql(const char* sql) {
        char* errMsg = 0;
        int rc = sqlite3_exec(db, sql, 0, 0, &errMsg);
        if (rc != SQLITE_OK) {
            std::cerr << "[SQLite Error] " << errMsg << "\nSQL: " << sql
                      << std::endl;
            sqlite3_free(errMsg);
        }
    }

   public:
    SQLiteStorage(const std::string& db_path) {
        int rc = sqlite3_open(db_path.c_str(), &db);
        if (rc) {
            std::cerr << "Can't open database: " << sqlite3_errmsg(db)
                      << std::endl;
            exit(1);
        }
        initTables();
    }

    ~SQLiteStorage() { sqlite3_close(db); }

    void initTables() {
        // 创建三张表：keys, pp, aux
        // OR REPLACE 语法是 SQLite 的特性，如果 ID 重复直接覆盖
        exec_sql(
            "CREATE TABLE IF NOT EXISTS users (id INTEGER PRIMARY KEY, pk "
            "BLOB);");
        exec_sql(
            "CREATE TABLE IF NOT EXISTS pp (block_id INTEGER PRIMARY KEY, "
            " commitment BLOB);");
        exec_sql(
            "CREATE TABLE IF NOT EXISTS aux (row_id INTEGER PRIMARY KEY,  "
            " upd "
            "BLOB);");
    }

    // --- 实现接口: isUserRegistered ---
    bool isUserRegistered(int id) override {
        sqlite3_stmt* stmt;
        std::string sql = "SELECT count(*) FROM users WHERE id = ?";
        sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0);
        sqlite3_bind_int(stmt, 1, id);

        bool exists = false;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            int count = sqlite3_column_int(stmt, 0);
            exists = (count > 0);
        }
        sqlite3_finalize(stmt);
        return exists;
    }

    // --- 实现接口: saveUserPublicKey ---
    void saveUserPublicKey(int id, const G1& pk) override {
        std::string blob = g1_to_bin(pk);
        sqlite3_stmt* stmt;
        // INSERT OR REPLACE: 如果 ID 存在则更新，不存在则插入
        std::string sql = "INSERT OR REPLACE INTO users (id, pk) VALUES (?, ?)";

        sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0);
        sqlite3_bind_int(stmt, 1, id);
        // 绑定二进制数据 (BLOB)
        sqlite3_bind_blob(stmt, 2, blob.c_str(), blob.size(), SQLITE_STATIC);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            std::cerr << "Error saving user pk: " << sqlite3_errmsg(db)
                      << std::endl;
        }
        sqlite3_finalize(stmt);
    }

    // --- 实现接口: getPPCommitment ---
    G1 getPPCommitment(int block_index) override {
        sqlite3_stmt* stmt;
        std::string sql = "SELECT commitment FROM pp WHERE block_id = ?";
        sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0);
        sqlite3_bind_int(stmt, 1, block_index);

        G1 result;
        result.clear();  // 默认设为 0

        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const void* data = sqlite3_column_blob(stmt, 0);
            int bytes = sqlite3_column_bytes(stmt, 0);
            std::string s((const char*)data, bytes);
            result = bin_to_g1(s);
        }
        sqlite3_finalize(stmt);
        return result;
    }

    // --- 实现接口: savePPCommitment ---
    void savePPCommitment(int block_index, const G1& new_com) override {
        std::string blob = g1_to_bin(new_com);
        sqlite3_stmt* stmt;
        std::string sql =
            "INSERT OR REPLACE INTO pp (block_id, commitment) VALUES (?, ?)";

        sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0);
        sqlite3_bind_int(stmt, 1, block_index);
        sqlite3_bind_blob(stmt, 2, blob.c_str(), blob.size(), SQLITE_STATIC);

        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    // --- 实现接口: getAuxUpdate ---
    G1 getAuxUpdate(int row_id) override {
        sqlite3_stmt* stmt;
        std::string sql = "SELECT upd FROM aux WHERE row_id = ?";
        sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0);
        sqlite3_bind_int(stmt, 1, row_id);

        G1 result;
        result.clear();

        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const void* data = sqlite3_column_blob(stmt, 0);
            int bytes = sqlite3_column_bytes(stmt, 0);
            std::string s((const char*)data, bytes);
            result = bin_to_g1(s);
        }
        sqlite3_finalize(stmt);
        return result;
    }

    // --- 实现接口: saveAuxUpdate ---
    void saveAuxUpdate(int row_id, const G1& upd) override {
        std::string blob = g1_to_bin(upd);
        sqlite3_stmt* stmt;
        std::string sql =
            "INSERT OR REPLACE INTO aux (row_id, upd) VALUES (?, ?)";

        sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0);
        sqlite3_bind_int(stmt, 1, row_id);
        sqlite3_bind_blob(stmt, 2, blob.c_str(), blob.size(), SQLITE_STATIC);

        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
};