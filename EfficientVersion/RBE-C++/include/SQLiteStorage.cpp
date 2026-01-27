#include <SQLiteStorage.h>

// 辅助：执行无返回值的 SQL (如 CREATE TABLE)
void SQLiteStorage::exec_sql(const char* sql) {
    char* errMsg = 0;
    int rc = sqlite3_exec(db, sql, 0, 0, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "[SQLite Error] " << errMsg << "\nSQL: " << sql
                  << std::endl;
        sqlite3_free(errMsg);
    }
}

SQLiteStorage::SQLiteStorage(const std::string& db_path) {
    int rc = sqlite3_open(db_path.c_str(), &db);
    if (rc) {
        std::cerr << "Can't open database: " << sqlite3_errmsg(db) << std::endl;
        exit(1);
    }
    initTables();
}

SQLiteStorage::~SQLiteStorage() { sqlite3_close(db); }

void SQLiteStorage::initTables() {
    // 创建四张表：keys, pp, aux, counts
    // OR REPLACE 语法是 SQLite 的特性，如果 ID 重复直接覆盖
    exec_sql(
        "CREATE TABLE IF NOT EXISTS users (id INTEGER PRIMARY KEY, pk "
        "BLOB);");
    exec_sql(
        "CREATE TABLE IF NOT EXISTS pp (block_id INTEGER, level INTEGER, "
        "commitment BLOB, PRIMARY KEY (block_id, level));");
    exec_sql(
        "CREATE TABLE IF NOT EXISTS aux (row_id INTEGER, level INTEGER, "
        "upd BLOB, PRIMARY KEY (row_id, level));");
    exec_sql(
        "CREATE TABLE IF NOT EXISTS counts (block_id INTEGER, level "
        "INTEGER, count INTEGER, PRIMARY KEY (block_id, level));");
}

// --- 实现接口: isUserRegistered ---
bool SQLiteStorage::isUserRegistered(int id) {
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
void SQLiteStorage::saveUserPublicKey(int id, const G1& pk) {
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
G1 SQLiteStorage::getPPCommitment(int block_index, int level) {
    sqlite3_stmt* stmt;
    std::string sql =
        "SELECT commitment FROM pp WHERE block_id = ? AND level = ?";
    sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, block_index);
    sqlite3_bind_int(stmt, 2, level);
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
void SQLiteStorage::savePPCommitment(int block_index, int level,
                                     const G1& com) {
    std::string blob = g1_to_bin(com);
    sqlite3_stmt* stmt;
    std::string sql =
        "INSERT OR REPLACE INTO pp (block_id, level, commitment) VALUES "
        "(?, ?, ?)";
    sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, block_index);
    sqlite3_bind_int(stmt, 2, level);
    sqlite3_bind_blob(stmt, 3, blob.c_str(), blob.size(), SQLITE_STATIC);

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

// --- 实现接口: deletePPCommitment ---
void SQLiteStorage::deletePPCommitment(int block_index, int level) {
    sqlite3_stmt* stmt;
    std::string sql = "DELETE FROM pp WHERE block_id = ? AND level = ?";
    sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, block_index);
    sqlite3_bind_int(stmt, 2, level);

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

// --- 实现接口: getAuxUpdate ---
G1 SQLiteStorage::getAuxUpdate(int row_id, int level) {
    sqlite3_stmt* stmt;
    std::string sql = "SELECT upd FROM aux WHERE row_id = ? AND level = ?";
    sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, row_id);
    sqlite3_bind_int(stmt, 2, level);
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
void SQLiteStorage::saveAuxUpdate(int row_id, int level, const G1& upd) {
    std::string blob = g1_to_bin(upd);
    sqlite3_stmt* stmt;
    std::string sql =
        "INSERT OR REPLACE INTO aux (row_id, level, upd) VALUES (?, ?, ?)";
    sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, row_id);
    sqlite3_bind_int(stmt, 2, level);
    sqlite3_bind_blob(stmt, 3, blob.c_str(), blob.size(), SQLITE_STATIC);

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

// --- 实现接口: deleteAuxUpdate ---
void SQLiteStorage::deleteAuxUpdate(int row_id, int level) {
    sqlite3_stmt* stmt;
    std::string sql = "DELETE FROM aux WHERE row_id = ? AND level = ?";
    sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, row_id);
    sqlite3_bind_int(stmt, 2, level);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

// --- 实现接口: getUserCountInLevel ---
int SQLiteStorage::getUserCountInLevel(int block_index, int level) {
    sqlite3_stmt* stmt;
    std::string sql =
        "SELECT count FROM counts WHERE block_id = ? AND level = ?";
    sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, block_index);
    sqlite3_bind_int(stmt, 2, level);
    int result = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return result;
}

// --- 实现接口: setUserCountInLevel ---
void SQLiteStorage::setUserCountInLevel(int block_index, int level, int count) {
    sqlite3_stmt* stmt;
    std::string sql =
        "INSERT OR REPLACE INTO counts (block_id, level, count) VALUES (?, "
        "?, ?)";
    sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, block_index);
    sqlite3_bind_int(stmt, 2, level);
    sqlite3_bind_int(stmt, 3, count);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

// --- 实现接口: hasAux ---
bool SQLiteStorage::hasAux(int row_id, int level) {
    sqlite3_stmt* stmt;
    std::string sql = "SELECT 1 FROM aux WHERE row_id = ? AND level = ?";
    sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, row_id);
    sqlite3_bind_int(stmt, 2, level);
    bool exists = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return exists;
}
