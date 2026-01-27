#include <iostream>
#include <string>
#include <utility>  // for std::pair

#include "RBE_Common.h"
#include "SQLiteStorage.h"
#include "algos.h"

// 辅助函数：生成一个合法的随机消息 (GT 元素)
GT gen_valid_msg(const CRS& crs) {
    Fr r;
    r.setRand();
    GT base_pair;
    mcl::bn::pairing(base_pair, crs.g1, crs.g2);
    GT msg;
    GT::pow(msg, base_pair, r);
    return msg;
}

// 辅助函数：打印消息的简略信息
void print_msg_info(const std::string& prefix, const GT& msg) {
    std::string s;
    msg.getStr(s);
    std::cout << prefix << ": " << s.substr(0, 15) << "..." << std::endl;
}

int main() {
    // 1. 初始化
    init_rbe_library();

    std::string db_file = "EfficientVersion/sqlite3_db/rbe_full_efficient.db";
    std::remove(db_file.c_str());  // 清理旧数据

    SQLiteStorage store(db_file);
    Storage* storage = &store;

    std::cout << "=== [Setup] System Init N=100 ===" << std::endl;
    CRS crs = setup(100);

    // ==========================================
    // 场景 1: 注册 User 0 和 User 1
    // 预期结果: 它们会发生合并，最终停留在 Level 1
    // ==========================================
    std::cout << "\n=== [Step 1] Register User 0 & 1 (Target: Level 1) ==="
              << std::endl;

    // User 0
    UserKeys k0 = gen(crs, 0);
    reg(crs, storage, 0, k0.pk, k0.xi);

    // User 1 (触发 Level 0 冲突 -> 合并至 Level 1)
    UserKeys k1 = gen(crs, 1);
    reg(crs, storage, 1, k1.pk, k1.xi);

    // 验证 User 0 当前状态
    // upd 函数返回 pair<Level, Aux>
    std::pair<int, G1> u0_info_v1 = upd(crs, storage, 0);
    std::cout << "-> User 0 Current Level: " << u0_info_v1.first
              << " (Expected: 1)" << std::endl;

    // ==========================================
    // 场景 2: 正常加密解密
    // User 0 使用 Level 1 的数据解密
    // ==========================================
    std::cout << "\n=== [Step 2] Encrypt/Decrypt Test (Level 1) ==="
              << std::endl;

    GT msg_1 = gen_valid_msg(crs);
    print_msg_info("Original Msg", msg_1);

    // Encrypt
    Ciphertext ct_1 = enc(crs, storage, 0, msg_1);
    std::cout << "-> Ciphertext created with " << ct_1.components.size()
              << " components." << std::endl;

    // Decrypt using u0_info_v1 (Level 1)
    DecResult res_1 = dec(crs, 0, k0.sk, u0_info_v1, ct_1);

    if (res_1.success && res_1.message == msg_1) {
        std::cout << "[SUCCESS] Decryption verified!" << std::endl;
    } else {
        std::cout << "[FAIL] Decryption failed!" << std::endl;
        return -1;
    }

    // ==========================================
    // 场景 3: 注册 User 2 和 User 3 (触发大规模合并)
    // 预期结果:
    // User 2 -> Level 0
    // User 3 -> 冲突 -> Level 1 冲突 -> Level 2
    // 所有用户 (0,1,2,3) 最终都应该搬家到 Level 2
    // ==========================================
    std::cout
        << "\n=== [Step 3] Register User 2 & 3 (Trigger Merge to Level 2) ==="
        << std::endl;

    UserKeys k2 = gen(crs, 2);
    reg(crs, storage, 2, k2.pk, k2.xi);
    UserKeys k3 = gen(crs, 3);
    reg(crs, storage, 3, k3.pk, k3.xi);

    std::cout << "-> Merge complete. Data should be at Level 2." << std::endl;

    // ==========================================
    // 场景 4: 安全性检查 (使用过期的 Aux)
    // 加密者使用最新的 PP (Level 2) 加密
    // User 0 尝试使用旧的 Aux (Level 1) 解密 -> 应该失败
    // ==========================================
    std::cout << "\n=== [Step 4] Stale Update Security Check ===" << std::endl;

    GT msg_2 = gen_valid_msg(crs);
    Ciphertext ct_2 = enc(crs, storage, 0, msg_2);  // Enc 使用最新的 DB 状态

    std::cout << "-> User 0 tries decrypting with OLD update (Level 1)..."
              << std::endl;
    // 注意：这里传入的是 u0_info_v1 (旧数据)
    DecResult res_2 = dec(crs, 0, k0.sk, u0_info_v1, ct_2);

    if (!res_2.success && res_2.need_update) {
        std::cout << "[SUCCESS] Decryption failed as expected (Need Update)."
                  << std::endl;
    } else {
        std::cout << "[FAIL] Security check failed! User shouldn't be able to "
                     "decrypt."
                  << std::endl;
        return -1;
    }

    // ==========================================
    // 场景 5: 获取更新并恢复服务
    // User 0 从服务器拉取 Level 2 的 Aux -> 解密成功
    // ==========================================
    std::cout << "\n=== [Step 5] Fetch Update & Retry ===" << std::endl;

    // 1. 获取更新
    std::pair<int, G1> u0_info_v2 = upd(crs, storage, 0);
    std::cout << "-> User 0 New Level: " << u0_info_v2.first << " (Expected: 2)"
              << std::endl;

    // 2. 解密
    DecResult res_3 = dec(crs, 0, k0.sk, u0_info_v2, ct_2);

    if (res_3.success && res_3.message == msg_2) {
        std::cout << "[SUCCESS] Decryption verified after update!" << std::endl;
        print_msg_info("Decrypted Msg", res_3.message);
    } else {
        std::cout << "[FAIL] Decryption failed even after update!" << std::endl;
        return -1;
    }

    std::cout << "\n=== ALL TESTS PASSED ===" << std::endl;
    return 0;
}