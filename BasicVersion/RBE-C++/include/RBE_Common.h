// RBE_Common.h
#pragma once
#include <cmath>
#include <iostream>
#include <vector>

#include "mcl/bls12_381.hpp"

// 使用 mcl 的命名空间，简化代码
using namespace mcl::bn;

// 全局初始化函数，必须在 main 开头调用
inline void init_rbe_library() { initPairing(mcl::BLS12_381); }

// 对应 Python 中的 objects.CRS
struct CRS {
    int N;  // 最大用户数
    int n;  // sqrt(N)
    G1 g1;  // G1 生成元
    G2 g2;  // G2 生成元

    // 对应 Python 的 h_parameters_g1 和 h_parameters_g2
    // h[i] = g^(z^i)
    std::vector<G1> h_g1;
    std::vector<G2> h_g2;

    // 构造函数：对应 Python setup 中的逻辑
    CRS(int max_users) : N(max_users) { n = std::ceil(std::sqrt(N)); }
};

// 对应 Python 中的 keys
struct UserKeys {
    Fr sk;               // 私钥 (标量)
    G1 pk;               // 公钥 (G1上的点)
    std::vector<G1> xi;  // 辅助值 (helping_values)，对应论文的 xi
};

struct Ciphertext {
    G1 ct0;  // 使用的承诺 (Commitment)
    GT ct1;
    G2 ct2;
    GT ct3;  // 包含消息的部分
};