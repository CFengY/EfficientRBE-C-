// #pragma once
// #include <Storage.h>

// #include <map>

// // ---------------------------------------------------------
// // 为了让你快速跑通算法逻辑，我们先写一个 "内存版" 的实现。
// // 等算法调通了，我们再写一个 "SQLite版" 的实现替换它。
// // 这样可以避免 SQL 语法错误干扰我们调试数学逻辑。
// // ---------------------------------------------------------
// class InMemoryStorage : public Storage {
//     std::map<int, G1> user_pks;
//     std::map<int, G1> pp_commitments;
//     std::map<int, G1> aux_updates;

//    public:
//     bool isUserRegistered(int id) override { return user_pks.count(id); }

//     void saveUserPublicKey(int id, const G1& pk) override { user_pks[id] =
//     pk; }

//     G1 getPPCommitment(int block_index) override {
//         if (pp_commitments.count(block_index)) {
//             return pp_commitments[block_index];
//         }
//         // 如果不存在，返回 0 (单位元)
//         G1 zero;
//         zero.clear();  // mcl 中 clear() 设置为无穷远点(加法单位元)
//         return zero;
//     }

//     void savePPCommitment(int block_index, const G1& new_com) override {
//         pp_commitments[block_index] = new_com;
//     }

//     G1 getAuxUpdate(int row_id) override {
//         if (aux_updates.count(row_id)) {
//             return aux_updates[row_id];
//         }
//         G1 zero;
//         zero.clear();
//         return zero;
//     }

//     void saveAuxUpdate(int row_id, const G1& upd) override {
//         aux_updates[row_id] = upd;
//     }
// };