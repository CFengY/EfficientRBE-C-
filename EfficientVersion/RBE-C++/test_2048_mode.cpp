#include <iomanip>  // 用于格式化输出

#include "RBE_Common.h"
#include "SQLiteStorage.h"
#include "algos.h"

// --- 辅助函数：查房 ---
// 打印指定 Block 中，各层级(Level)的占用情况
void inspect_db(Storage* storage, int block_id, int max_level) {
    std::cout << "\n[Database Status] Block " << block_id << ":" << std::endl;
    std::cout << "------------------------------------------------"
              << std::endl;
    std::cout << "| Level | Count | Has PP? | Status             |"
              << std::endl;
    std::cout << "------------------------------------------------"
              << std::endl;

    for (int lvl = 0; lvl <= max_level; ++lvl) {
        int count = storage->getUserCountInLevel(block_id, lvl);

        // 检查这一层有没有 Commitment
        G1 com = storage->getPPCommitment(block_id, lvl);
        bool has_pp = !com.isZero();  // 如果不是 0 (无穷远点)

        std::cout << "| " << std::setw(5) << lvl << " | " << std::setw(5)
                  << count << " | " << std::setw(7) << (has_pp ? "Yes" : "No")
                  << " | ";

        if (count == 0) {
            std::cout << "Empty";
        } else {
            // 根据 2048 规则：
            // Level 0 容量应为 1
            // Level 1 容量应为 2
            // Level 2 容量应为 4
            int expected = std::pow(2, lvl);
            if (count == expected) {
                std::cout << "Full (" << count << " users)";
            } else {
                std::cout << "Partial (" << count << "/" << expected << ")";
            }
        }
        std::cout << "|" << std::endl;
    }
    std::cout << "------------------------------------------------"
              << std::endl;
}

int main() {
    init_rbe_library();

    // 1. 准备环境
    std::string db_file = "EfficientVersion/sqlite3_db/rbe_efficient_test.db";
    std::remove(db_file.c_str());  // 清空旧数据
    SQLiteStorage store(db_file);
    Storage* storage = &store;

    int N = 100;
    CRS crs = setup(N);

    // 我们主要测试 Block 0，所以所有 ID 都在 0-9 之间
    int block_id = 0;

    // --- 阶段 1: 注册第 1 个用户 ---
    std::cout << "\n\n=== 1. Registering User 0 ===" << std::endl;
    UserKeys k0 = gen(crs, 0);
    reg(crs, storage, 0, k0.pk, k0.xi);
    // 预期: Level 0 有人
    inspect_db(storage, block_id, 3);

    // --- 阶段 2: 注册第 2 个用户 (触发第一次合并) ---
    std::cout << "\n\n=== 2. Registering User 1 ===" << std::endl;
    UserKeys k1 = gen(crs, 1);
    reg(crs, storage, 1, k1.pk, k1.xi);
    // 预期: Level 0 空了, Level 1 有人 (2人)
    inspect_db(storage, block_id, 3);

    // --- 阶段 3: 注册第 3 个用户 ---
    std::cout << "\n\n=== 3. Registering User 2 ===" << std::endl;
    UserKeys k2 = gen(crs, 2);
    reg(crs, storage, 2, k2.pk, k2.xi);
    // 预期: Level 0 有人, Level 1 也有人
    inspect_db(storage, block_id, 3);

    // --- 阶段 4: 注册第 4 个用户 (触发连锁合并) ---
    std::cout << "\n\n=== 4. Registering User 3 ===" << std::endl;
    UserKeys k3 = gen(crs, 3);
    reg(crs, storage, 3, k3.pk, k3.xi);
    // 预期:
    // User 3 进 Level 0 -> 冲突 (和 User 2 合并) -> 变成 Level 1 块
    // 新 Level 1 块 -> 冲突 (和之前的 U0+U1 合并) -> 变成 Level 2 块
    // 最终: Level 0 空, Level 1 空, Level 2 有人 (4人)
    inspect_db(storage, block_id, 3);

    // --- 验证 Aux 是否存在 ---
    // 此时这 4 个用户应该都在 Level 2 的 Aux 数据里
    std::cout << "\n\n=== Verifying Aux Data Location ===" << std::endl;
    bool all_good = true;
    for (int id = 0; id <= 3; ++id) {
        bool has_L2 = storage->hasAux(id, 2);
        bool has_L0 = storage->hasAux(id, 0);
        bool has_L1 = storage->hasAux(id, 1);

        std::cout << "User " << id << ": L0=" << has_L0 << ", L1=" << has_L1
                  << ", L2=" << has_L2 << std::endl;

        if (!has_L2 || has_L0 || has_L1) {
            all_good = false;
        }
    }

    if (all_good) {
        std::cout << "[SUCCESS] All users correctly moved to Level 2!"
                  << std::endl;
    } else {
        std::cout << "[FAIL] Aux data is scattered incorrectly." << std::endl;
    }

    return 0;
}