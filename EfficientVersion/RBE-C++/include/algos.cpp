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
        // 错误代码: if (i == crs.n) {
        // 修正为: 跳过 n+1
        if (i == crs.n + 1) {
            // h_{n+1} 是不需要的，因为它是 self-contribution 的基底
            // 保持 h_g1[i] 为 0
        } else {
            // h_g1[i] = g1 * z^i
            G1::mul(crs.h_g1[i], crs.g1, z_pow);
            // h_g2[i] = g2 * z^i
            G2::mul(crs.h_g2[i], crs.g2, z_pow);
        }

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
    // 1. 基础检查与存储
    if (storage->isUserRegistered(id)) return;
    storage->saveUserPublicKey(id, pk);

    int n = crs.n;
    int k = std::floor(id / n);  // 块号
    int id_rel = id % n;         // 块内相对位置

    // --- 准备初始数据 (Level -1) ---
    G1 current_com = pk;

    // 关键改变：我们维护整个块的辅助值向量 (大小为 n)
    // 初始状态下，这就是用户生成的 helping_values
    // 注意：用户不给自己提供 helping_value，所以 helping_values[id_rel] 应该是
    // 0 或无效值 这里的 current_aux_vec[i] 代表：当前这个(些)用户给位置 i
    // 的人的贡献总和
    std::vector<G1> current_aux_vec = helping_values;

    // 确保自己给自己位置的贡献是 0 (虽然 gen 里面可能已经是了，为了安全起见)
    current_aux_vec[id_rel].clear();

    int level = 0;

    // --- 2048 风格合并循环 ---
    while (true) {
        // 1. 检查冲突
        int count = storage->getUserCountInLevel(k, level);

        if (count == 0) {
            // --- 空位，落座 ---
            // A. 存入 Commitment
            storage->savePPCommitment(k, level, current_com);
            storage->setUserCountInLevel(
                k, level, 1);  // 这里 count 仅仅是个标记，设为 1 即可

            // B. 存入 Aux 向量 (存入所有 n 个位置！)
            // 无论这些位置上有没有人注册，都要存！因为这是为了未来合并准备的。
            for (int i = 0; i < n; ++i) {
                // 计算全局 row_id
                int target_row = k * n + i;
                storage->saveAuxUpdate(target_row, level, current_aux_vec[i]);
            }

            std::cout << "[Reg] Settled at Block " << k << " Level " << level
                      << std::endl;
            break;
        }

        // --- 冲突，合并 ---
        std::cout << "[Reg] Collision at Level " << level << ". Merging..."
                  << std::endl;

        // 2. 获取旧数据 (Old Group)
        G1 old_com = storage->getPPCommitment(k, level);

        // 读取旧的 Aux 向量 (全部 n 个)
        std::vector<G1> old_aux_vec(n);
        for (int i = 0; i < n; ++i) {
            int target_row = k * n + i;
            // 必须保证 hasAux 为真，或者 getAuxUpdate 返回 0
            old_aux_vec[i] = storage->getAuxUpdate(target_row, level);
        }

        // 3. 执行合并 (Merge)
        // Commitment 相加
        G1::add(current_com, current_com, old_com);

        // Aux 向量对应位置相加 (Component-wise Addition)
        for (int i = 0; i < n; ++i) {
            G1::add(current_aux_vec[i], current_aux_vec[i], old_aux_vec[i]);
        }

        // 4. 清理旧层级
        storage->deletePPCommitment(k, level);
        storage->setUserCountInLevel(k, level, 0);
        // 删除旧层级的所有 Aux 数据
        for (int i = 0; i < n; ++i) {
            int target_row = k * n + i;
            storage->deleteAuxUpdate(target_row, level);
        }

        // 5. 晋升
        level++;
    }
}

// 加密函数
// message: 这里假设消息 m 本身就是 GT 上的一个元素 (为了简化)
Ciphertext enc(const CRS& crs, Storage* storage, int id, const GT& message) {
    Ciphertext final_ct;
    int n = crs.n;
    int k = std::floor(id / n);
    int id_index = id % n;

    // 辅助参数准备
    int h_idx_g2 = n - id_index;
    const G2& h_term_g2 = crs.h_g2[h_idx_g2];
    const G1& h_id_g1 = crs.h_g1[id_index + 1];

    // 遍历所有可能的层级
    int max_level = std::ceil(std::log2(n)) + 2;

    for (int lvl = 0; lvl <= max_level; ++lvl) {
        // 1. 获取该层的 Commitment
        G1 com = storage->getPPCommitment(k, lvl);

        // 如果是 0，说明这一层没数据，跳过
        if (com.isZero()) continue;

        // 2. 针对这一层进行加密 (和 Base RBE 逻辑一样)
        CiphertextComponent comp;
        comp.level = lvl;
        comp.ct0 = com;

        Fr r;
        r.setRand();

        // ct1 = e(com, h_term)^r
        GT pair_val;
        mcl::bn::pairing(pair_val, com, h_term_g2);
        GT::pow(comp.ct1, pair_val, r);

        // ct2 = g2^r
        G2::mul(comp.ct2, crs.g2, r);

        // ct3 = e(h_id, h_term)^r * m
        GT e_val, e_val_r;
        mcl::bn::pairing(e_val, h_id_g1, h_term_g2);
        GT::pow(e_val_r, e_val, r);
        GT::mul(comp.ct3, e_val_r, message);

        // 加入列表
        final_ct.components.push_back(comp);
    }

    return final_ct;
}

std::pair<int, G1> upd(const CRS& crs, Storage* storage, int id) {
    // 我们的 Storage 没有直接提供 "find level by id" 的接口。
    // 但我们可以遍历 Level 0 到 log N。对于 N=100，最多也就 7 层，很快。

    int k = std::floor(id / crs.n);
    int max_level = std::ceil(std::log2(crs.n)) + 2;  // 稍微多扫几层防万一

    for (int lvl = 0; lvl <= max_level; ++lvl) {
        if (storage->hasAux(id, lvl)) {
            // 找到了！用户在这一层
            return {lvl, storage->getAuxUpdate(id, lvl)};
        }
    }

    // 没找到
    G1 zero;
    zero.clear();
    return {-1, zero};
}

DecResult dec(const CRS& crs, int id, const Fr& sk,
              const std::pair<int, G1>& user_upd_info, const Ciphertext& ct) {
    DecResult res;

    int my_level = user_upd_info.first;
    G1 my_aux = user_upd_info.second;

    if (my_level == -1) {
        // 用户根本不在系统里，或者数据丢了
        res.success = false;
        res.need_update = true;  // 其实是 Fatal Error
        return res;
    }

    // 1. 在密文列表中寻找匹配 Level 的分量
    const CiphertextComponent* target_comp = nullptr;
    for (const auto& comp : ct.components) {
        if (comp.level == my_level) {
            target_comp = &comp;
            break;
        }
    }

    if (target_comp == nullptr) {
        // 没找到对应层的密文。
        // 这通常意味着：加密发生时，用户所在的层级还没有人（或者已经被清空了）。
        // 即：用户的数据过时了 (Need Update)。
        res.success = false;
        res.need_update = true;
        return res;
    }

    // 2. 使用 Base RBE 的逻辑解密 target_comp
    // ... 代码逻辑和之前完全一样，只是把 ct.ct0 换成 target_comp->ct0 等等 ...

    int n = crs.n;
    int id_index = id % n;
    int h_idx_g2 = n - id_index;
    const G2& h_term_g2 = crs.h_g2[h_idx_g2];

    // 验证公式
    GT lhs;
    mcl::bn::pairing(lhs, target_comp->ct0, h_term_g2);

    G1 my_pk;
    G1::mul(my_pk, crs.h_g1[id_index + 1], sk);

    GT rhs_part1, rhs_part2, rhs;
    mcl::bn::pairing(rhs_part1, my_aux, crs.g2);
    mcl::bn::pairing(rhs_part2, my_pk, h_term_g2);
    GT::mul(rhs, rhs_part1, rhs_part2);

    if (lhs != rhs) {
        res.success = false;
        res.need_update = true;
        return res;
    }

    // 解密
    GT A, A_inv, B, C, C_inv;
    Fr sk_inv;

    mcl::bn::pairing(A, my_aux, target_comp->ct2);
    GT::inv(A_inv, A);
    GT::mul(B, A_inv, target_comp->ct1);
    Fr::inv(sk_inv, sk);
    GT::pow(C, B, sk_inv);
    GT::inv(C_inv, C);
    GT::mul(res.message, target_comp->ct3, C_inv);

    res.success = true;
    res.need_update = false;
    return res;
}