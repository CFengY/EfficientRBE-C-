// Storage.h
#pragma once
#include <RBE_Common.h>

// 序列化模式：使用 mcl 的二进制序列化，节省空间
const int SER_MODE = mcl::IoSerialize;

class Storage {
   public:
    virtual ~Storage() = default;

    // --- 1. 用户 Key 相关 ---
    // 检查用户是否已注册
    virtual bool isUserRegistered(int id) = 0;
    // 存储用户公钥 (模拟 Key Curator 收到 PK)
    virtual void saveUserPublicKey(int id, const G1& pk) = 0;

    // --- 2. Public Parameters (PP) 相关 ---
    // 获取第 k 个块的承诺 (Commitment)
    // 如果该块还没人注册，应该返回群的单位元 (Identity Element)
    virtual G1 getPPCommitment(int block_index) = 0;

    // 更新/存储第 k 个块的承诺
    virtual void savePPCommitment(int block_index, const G1& new_com) = 0;

    // --- 3. Auxiliary Info (Aux) 相关 ---
    // 获取某一行(row_id)的辅助信息
    virtual G1 getAuxUpdate(int row_id) = 0;

    // 更新/存储某一行(row_id)的辅助信息
    virtual void saveAuxUpdate(int row_id, const G1& upd) = 0;
};