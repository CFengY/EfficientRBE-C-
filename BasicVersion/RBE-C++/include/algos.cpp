#include "algos.h"

#include <cmath>

CRS setup(int N) {
    CRS crs(N);

    // --- 修改开始 ---
    // 错误代码: G1::getGen(crs.g1);
    // 错误代码: G2::getGen(crs.g2);

    // 正确做法：将固定的种子字符串映射到曲线上作为生成元
    // 只要字符串一样，生成的 crs.g1 和 crs.g2 就是一样的
    hashAndMapToG1(crs.g1, "generator_g1", 12);
    hashAndMapToG2(crs.g2, "generator_g2", 12);
    // --- 修改结束 ---

    // 2. 生成随机陷门 z (Master Trapdoor)
    Fr z;
    z.setRand();

    // 3. 计算 h 参数 (保持之前的逻辑不变)
    int limit = 2 * crs.n;
    crs.h_g1.resize(limit + 1);
    crs.h_g2.resize(limit + 1);

    Fr z_pow = z;  // 初始为 z^1

    for (int i = 1; i <= limit; ++i) {
        if (i == crs.n) {
            // 第 n 项留空，不进行计算
            // 注意：mcl 的 vector resize 后默认初始化为 0
            // (无穷远点)，所以直接跳过即可
        } else {
            // h_g1[i] = g1 * z^i
            G1::mul(crs.h_g1[i], crs.g1, z_pow);
            // h_g2[i] = g2 * z^i
            G2::mul(crs.h_g2[i], crs.g2, z_pow);
        }

        // 递推下一项幂： z^(i+1) = z^i * z
        z_pow *= z;
    }

    std::cout << "[Setup] CRS generated for N=" << N << ", n=" << crs.n
              << std::endl;
    return crs;
}

// id 是用户身份 (0 到 N-1)
UserKeys gen(const CRS& crs, int id) {
    UserKeys keys;

    // 1. 身份映射
    // Python: id_index = mod(id, crs.n)
    // 还要注意：Python代码里很多索引是 +1 或 -1 的，要非常小心对照
    // 原文: h_id_index = crs.h_parameters_g1[id_index]
    // 但原文 setup 里 h 是从下标 1 开始填的，所以这里我们假设 C++ vector
    // 下标直接对应公式下标

    int id_index = (id % crs.n) + 1;  // 调整为 1-based index 对应 h 列表

    // 2. 生成私钥 sk
    keys.sk.setRand();

    // 3. 计算公钥 pk = h_{id_index} ^ sk
    // mcl: G1::mul(out, point, scalar)
    if (id_index >= crs.h_g1.size()) {
        std::cerr << "Error: id_index out of bounds" << std::endl;
        exit(1);
    }
    G1::mul(keys.pk, crs.h_g1[id_index], keys.sk);

    // 4. 计算 helping values (xi)
    // Python: helping_values[i] = crs.h_parameters_g1[id_index+j+1]**sk
    // 循环 j from 0 to n-1

    keys.xi.resize(crs.n);

    for (int j = 0; j < crs.n; j++) {
        // Python逻辑: i = crs.n - 1 - j (倒序填充)
        int i = crs.n - 1 - j;

        // 目标 h 的索引
        int target_h_idx = (id % crs.n) + j +
                           2;  // +1(因为id_index) + j + 1(公式) = id%n + j + 2
        // *注意*：这里一定要根据 Python 的 `id_index + j + 1` 仔细校对
        // Python id_index 是 0-based result of mod.
        // 让我们重新核对一下 Python 源码:
        // id_index = mod(id, crs.n)  -> 0 to n-1
        // h_id_index = crs.h[id_index] -> Python list index.
        // 但是 Setup 里 Python 也是 h_values[i+1]，所以 Python list index 0
        // 存的是 h_1? 不，Python setup里 `h_values_crs1[i] = ...` i 从 0 到
        // 2n-1. `z.mod_pow(i+1)` 说明 list[0] 是 h_1, list[1] 是 h_2.

        // 所以:
        // id_index (0-based) 对应 h_{id_index+1}
        // helping value 取的是 h[id_index + j + 1] -> 对应数学上的 h_{id_index
        // + j + 2}

        // 修正后的 C++ 索引逻辑：
        int python_list_idx = (id % crs.n) + j + 1;

        // 对应我们 C++ vector (如果此时 vector[1] 是 h_1):
        // 那么我们要取 vector[python_list_idx + 1]
        int vec_idx = python_list_idx + 1;

        if (vec_idx >= crs.h_g1.size() ||
            vec_idx == crs.n + 1) {  // 也就是 h_{n+1}
            // Python: if h... == None: continue
            // 对应 h_{n+1} 是空的
            continue;
        }

        // xi[i] = h[vec_idx] ^ sk
        G1::mul(keys.xi[i], crs.h_g1[vec_idx], keys.sk);
    }

    return keys;
}

// 注册函数
// 参数：
// crs: 公共参考串
// storage: 数据库接口指针
// id: 用户 ID
// pk: 用户公钥
// helping_values: 用户生成的辅助值列表 (xi)
void reg(const CRS& crs, Storage* storage, int id, const G1& pk,
         const std::vector<G1>& helping_values) {
    // 1. 简单的重复注册检查
    if (storage->isUserRegistered(id)) {
        std::cerr << "User " << id << " already registered!" << std::endl;
        return;
    }
    storage->saveUserPublicKey(id, pk);

    // 2. 计算块索引 (Block Index)
    // 论文逻辑: k = floor(id / n)
    int k = std::floor(id / crs.n);
    int id_index_in_block = id % crs.n;  // 在当前块内的偏移量

    // ==========================================
    // 3. 更新 Public Parameter (PP)
    // ==========================================

    // 获取当前块的旧承诺
    G1 old_com = storage->getPPCommitment(k);

    // 计算新承诺： New = Old + PK (注意 mcl 是加法表示)
    G1 new_com;
    G1::add(new_com, old_com, pk);

    // 存回去
    storage->savePPCommitment(k, new_com);

    // ==========================================
    // 4. 更新 Auxiliary Info (Aux)
    // 这是最耗时的部分：O(sqrt(N))
    // ==========================================

    // 遍历当前块内的所有位置 (除了注册者自己)
    for (int i = 0; i < crs.n; ++i) {
        // 计算全局 ID
        int current_id = k * crs.n + i;

        // 不需要更新注册者自己的 Aux，因为他是新来的
        if (current_id == id) continue;

        // 计算数据库行号 (rowid)
        // Python逻辑: j = k * (n^2) + (i*n) ... 这里我们简化一下
        // 我们在 Storage 接口里抽象了 row_id。
        // 为了简单，我们定义 aux 表的 row_id 映射规则：
        // 第 k 个块，第 i 个用户的 Aux 存储位置 = k * n + i
        //
        // 仔细看 Python `reg` 函数逻辑：
        // 它实际上维护了一个 append-only 的 log。
        // 但对于基础版 (Base RBE)，我们只需要存储 "最新" 的 Aux 值。
        // Python 代码里 `efficient=False` 分支里：
        // `cur_aux.execute(" INSERT INTO aux(rowid, upd) ...")`
        // 它是 append 的。但为了我们 C++ 实现简单，且我们用的是 KV 存储 (Map)，
        // 我们直接覆盖更新 (Update in place)，效果是一样的，只是不能回溯历史。

        int row_id = current_id;

        // 1. 取出旧的 Aux
        G1 last_upd = storage->getAuxUpdate(row_id);

        // 2. 加上新用户的 helping value
        // 注意：helping_values 数组的索引。
        // 在 Gen 函数里，我们生成的 helping_values[i] 对应的是
        // h_parameters[...]. Python代码: `new_aux_value = last_upd *
        // helping_values[i]` 这里的 i 对应的是 `crs.n` 的循环变量。

        G1 new_upd;
        // 如果 helping_values[i] 是空的（比如 vector 初始化问题），要小心
        if (i < helping_values.size()) {
            // New_Aux = Old_Aux + Xi[i]
            G1::add(new_upd, last_upd, helping_values[i]);

            // 3. 存回去
            storage->saveAuxUpdate(row_id, new_upd);
        }
    }

    std::cout << "[Reg] User " << id << " registered in block " << k
              << std::endl;
}

// 加密函数
// message: 这里假设消息 m 本身就是 GT 上的一个元素 (为了简化)
Ciphertext enc(const CRS& crs, Storage* storage, int id, const GT& message) {
    Ciphertext ct;
    int n = crs.n;

    // 1. 确定 Block 和 ID 索引
    // 注意：id_index 是 0-based，用来取 h_parameters
    // Python: h_parameters[n-1-id_index]
    int k = std::floor(id / n);
    int id_index = id % n;

    // 2. 获取该 Block 的承诺
    // ct0 = Commitment
    ct.ct0 = storage->getPPCommitment(k);

    // 3. 生成随机数 r (在 Fr 域上)
    Fr r;
    r.setRand();

    // 4. 准备辅助变量 h_term (来自 G2)
    // Python: h_parameters_g2[n - 1 - id_index]
    // 我们的 h_g2 是 1-based (下标0是占位或h1)，需要仔细对齐
    // Python setup: h[i] i from 0 to 2n-1. h[n] is empty.
    // 我们 C++: h_g2[1] ... h_g2[2n].
    // 转换公式: Python Index + 1 = C++ Index
    // Python: n - 1 - id_index
    // C++: (n - 1 - id_index) + 1 = n - id_index
    int h_idx_g2 = n - id_index;
    const G2& h_term_g2 = crs.h_g2[h_idx_g2];

    // 5. 计算 ct1 = e(ct0, h_term)^r
    // mcl pairing: pairing(out, g1, g2)
    GT pair_val;
    mcl::bn::pairing(pair_val, ct.ct0, h_term_g2);
    // GT::pow(out, base, exp)
    GT::pow(ct.ct1, pair_val, r);

    // 6. 计算 ct2 = g2^r
    G2::mul(ct.ct2, crs.g2, r);

    // 7. 计算 ct3 = e(h_id, h_term)^r * m
    // h_id 在 C++ 的下标是 id_index + 1
    const G1& h_id_g1 = crs.h_g1[id_index + 1];

    GT e_val;
    mcl::bn::pairing(e_val, h_id_g1, h_term_g2);  // e(h_id, h_term)
    GT e_val_r;
    GT::pow(e_val_r, e_val, r);  // ^r

    GT::mul(ct.ct3, e_val_r, message);  // * m

    return ct;
}

// 获取更新
G1 upd(const CRS& crs, Storage* storage, int id) {
    // 逻辑和 Reg 里找 row_id 一样
    // Base RBE 中，row_id 就是用户的全局 id
    return storage->getAuxUpdate(id);
}

DecResult dec(const CRS& crs, int id, const Fr& sk, const G1& user_upd,
              const Ciphertext& ct) {
    DecResult res;
    int n = crs.n;
    int id_index = id % n;
    int h_idx_g2 = n - id_index;  // 对应 Enc 中的 h_term_g2 下标

    const G2& h_term_g2 = crs.h_g2[h_idx_g2];

    // --- 1. 验证阶段 (Check) ---
    // LHS (左边) = e(ct0, h_term)
    GT lhs;
    mcl::bn::pairing(lhs, ct.ct0, h_term_g2);

    // RHS (右边) = e(upd, g2) * e(h_id^sk, h_term)
    // 这里的 h_id^sk 其实就是用户的 pk。但根据函数签名，我们只有 sk。
    // 我们可以现算 pk，或者假设用户保存了自己的 pk。
    // 这里我们现算: pk = h[id_index+1] ^ sk
    G1 my_pk;
    G1::mul(my_pk, crs.h_g1[id_index + 1], sk);

    GT rhs_part1, rhs_part2;
    mcl::bn::pairing(rhs_part1, user_upd, crs.g2);  // e(upd, g2)
    mcl::bn::pairing(rhs_part2, my_pk, h_term_g2);  // e(pk, h_term)

    GT rhs;
    GT::mul(rhs, rhs_part1, rhs_part2);  // part1 * part2

    if (lhs != rhs) {
        // 验证失败，说明承诺 (ct0) 变了，但用户的 upd 还是旧的
        res.success = false;
        res.need_update = true;
        return res;
    }

    // --- 2. 解密阶段 ---
    // m = ct3 / ( e(upd, ct2)^(-1) * ct1 )^(sk^-1) ... 这里的公式有点绕
    // 让我们看 Python 代码最直接：
    // m = ct.ct3 /
    // ((u.pair(ct.ct2)**(-1)*(ct.ct1))**(sk.mod_pow(-1,GT.order()))) 简化一下：
    // Denom = ( e(upd, ct2)^-1 * ct1 ) ^ (1/sk)
    // Msg = ct3 / Denom

    // A = e(upd, ct2)
    GT A;
    mcl::bn::pairing(A, user_upd, ct.ct2);

    // A_inv = 1 / A
    GT A_inv;
    GT::inv(A_inv, A);

    // B = A_inv * ct1
    GT B;
    GT::mul(B, A_inv, ct.ct1);

    // sk_inv = 1 / sk (在 Fr 域上求逆)
    Fr sk_inv;
    Fr::inv(sk_inv, sk);

    // C = B ^ sk_inv
    GT C;
    GT::pow(C, B, sk_inv);

    // m = ct3 * (1/C)  (即 ct3 / C)
    GT C_inv;
    GT::inv(C_inv, C);

    GT::mul(res.message, ct.ct3, C_inv);

    res.success = true;
    res.need_update = false;
    return res;
}