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
    void exec_sql(const char* sql);

   public:
    SQLiteStorage(const std::string& db_path);

    ~SQLiteStorage();

    void initTables();

    // --- 接口: isUserRegistered ---
    bool isUserRegistered(int id) override;

    // --- 接口: saveUserPublicKey ---
    void saveUserPublicKey(int id, const G1& pk) override;

    // --- 接口: getPPCommitment ---
    G1 getPPCommitment(int block_index, int level) override;

    // --- 接口: savePPCommitment ---
    void savePPCommitment(int block_index, int level, const G1& com) override;

    // --- 接口: deletePPCommitment ---
    void deletePPCommitment(int block_index, int level) override;

    // --- 接口: getAuxUpdate ---
    G1 getAuxUpdate(int row_id, int level) override;

    // --- 接口: saveAuxUpdate ---
    void saveAuxUpdate(int row_id, int level, const G1& upd) override;

    // --- 接口: deleteAuxUpdate ---
    void deleteAuxUpdate(int row_id, int level) override;

    // --- 接口: getUserCountInLevel ---
    int getUserCountInLevel(int block_index, int level) override;

    // --- 接口: setUserCountInLevel ---
    void setUserCountInLevel(int block_index, int level, int count) override;

    // --- 接口: hasAux ---
    bool hasAux(int row_id, int level) override;
};