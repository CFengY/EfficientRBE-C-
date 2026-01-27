// #include "InMemoryStorage.h"
#include "RBE_Common.h"
#include "SQLiteStorage.h"
#include "algos.h"

int main() {
    init_rbe_library();

    // 准备数据库
    std::string db_file = "BasicVersion/sqlite3_db/rbe_full_test.db";
    std::remove(db_file.c_str());
    SQLiteStorage store(db_file);
    Storage* storage = &store;

    int N = 100;
    std::cout << "[Step 1] Setup..." << std::endl;
    CRS crs = setup(N);

    // --- 1. User A (ID=5) 进场 ---
    int id_a = 5;
    std::cout << "\n[Step 2] User A (ID=5) generating keys..." << std::endl;
    UserKeys key_a = gen(crs, id_a);

    std::cout << "[Step 3] User A registering..." << std::endl;
    reg(crs, storage, id_a, key_a.pk, key_a.xi);

    // --- 2. User A 尝试加密给自己 ---
    std::cout << "\n[Step 4] Encrypting message for User A..." << std::endl;
    GT msg_original;
    // 1. 生成一个随机标量 r
    Fr r_msg;
    r_msg.setRand();

    // 2. 计算基准配对 e(g1, g2)
    GT base_pair;
    mcl::bn::pairing(base_pair, crs.g1, crs.g2);

    // 3. 计算 m = e(g1, g2)^r
    // 这样生成的 m 绝对是 GT 群里的合法元素，而且是随机的
    GT::pow(msg_original, base_pair, r_msg);

    // 这里为了演示，我们打印一下消息的某种特征(比如转成string的一小段)
    std::string msg_str;
    msg_original.getStr(msg_str);
    std::cout << "Original Msg: " << msg_str.substr(0, 15) << "..."
              << std::endl;

    Ciphertext ct1 = enc(crs, storage, id_a, msg_original);

    // --- 3. User A 第一次解密 (应该成功) ---
    // User A 先从本地(或服务器)拿自己的 upd，目前应该是初始值
    G1 upd_a = upd(crs, storage, id_a);

    std::cout << "[Step 5] User A decrypting..." << std::endl;
    DecResult res1 = dec(crs, id_a, key_a.sk, upd_a, ct1);

    if (res1.success && res1.message == msg_original) {
        std::cout << "-> Decryption SUCCESS!" << std::endl;
    } else {
        std::cout << "-> Decryption FAILED!" << std::endl;
    }

    // --- 4. User B (ID=8) 进场，搞乱 Block 0 ---
    std::cout << "\n[Step 6] User B (ID=8) registering (Same Block)..."
              << std::endl;
    int id_b = 8;
    UserKeys key_b = gen(crs, id_b);
    reg(crs, storage, id_b, key_b.pk, key_b.xi);
    // 此时，Block 0 的承诺变了！User A 手里的 upd
    // 也过时了(数据库里已经变了，但A手里拿着旧的)。

    // --- 5. 再次加密给 User A ---
    // 注意：加密者用的是最新的 PP (因为 Enc 会读 storage->getPPCommitment)
    std::cout
        << "\n[Step 7] Encrypting NEW message for User A (with updated PP)..."
        << std::endl;
    Ciphertext ct2 = enc(crs, storage, id_a, msg_original);

    // --- 6. User A 用旧的 upd 尝试解密 (应该失败) ---
    std::cout << "[Step 8] User A decrypting with OLD update..." << std::endl;
    DecResult res2 =
        dec(crs, id_a, key_a.sk, upd_a, ct2);  // 注意这里传入的是旧的 upd_a

    if (!res2.success && res2.need_update) {
        std::cout << "-> Decryption FAILED as expected (Need Update)."
                  << std::endl;
    } else {
        std::cout << "-> Error: Should have failed but didn't!" << std::endl;
    }

    // --- 7. User A 获取更新并解密 ---
    std::cout << "\n[Step 9] User A fetching update..." << std::endl;
    G1 new_upd_a = upd(crs, storage, id_a);  // 从 DB 拉取最新的

    std::cout << "[Step 10] User A decrypting with NEW update..." << std::endl;
    DecResult res3 = dec(crs, id_a, key_a.sk, new_upd_a, ct2);

    if (res3.success && res3.message == msg_original) {
        std::cout << "-> Decryption SUCCESS after update!" << std::endl;
    } else {
        std::cout << "-> Decryption FAILED even after update!" << std::endl;
    }

    return 0;
}