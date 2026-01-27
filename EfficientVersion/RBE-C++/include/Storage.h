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

    // --- PP 相关 (增加了 level 参数) ---
    // 获取某块、某层的承诺
    virtual G1 getPPCommitment(int block_index, int level) = 0;
    // 存储
    virtual void savePPCommitment(int block_index, int level,
                                  const G1& com) = 0;
    // 删除（合并后旧层级要清空）
    virtual void deletePPCommitment(int block_index, int level) = 0;

    // --- Aux 相关 (增加了 level 参数) ---
    virtual G1 getAuxUpdate(int row_id, int level) = 0;
    virtual void saveAuxUpdate(int row_id, int level, const G1& upd) = 0;
    virtual void deleteAuxUpdate(int row_id, int level) = 0;

    // --- 计数器 (Helper) ---
    // 我们需要知道某个层级当前有没有东西，或者有多少人
    // 对应 Python 中的 pp_com_count 表
    virtual int getUserCountInLevel(int block_index, int level) = 0;
    virtual void setUserCountInLevel(int block_index, int level, int count) = 0;

    // 检查某行某层是否有Aux数据
    virtual bool hasAux(int row_id, int level) = 0;
};