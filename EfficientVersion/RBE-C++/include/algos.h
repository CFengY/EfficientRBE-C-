#pragma once
#include <map>
#include <set>

#include "RBE_Common.h"
#include "Storage.h"

CRS setup(int N);

// id 是用户身份 (0 到 N-1)
UserKeys gen(const CRS& crs, int id);

void reg(const CRS& crs, Storage* storage, int id, const G1& pk,
         const std::vector<G1>& helping_values);

Ciphertext enc(const CRS& crs, Storage* storage, int id, const GT& message);

std::pair<int, G1> upd(const CRS& crs, Storage* storage, int id);

// 解密结果结构体
struct DecResult {
    bool success;      // 是否成功
    bool need_update;  // 是否需要更新 (如果 success=false)
    GT message;        // 解密出的消息
};

DecResult dec(const CRS& crs, int id, const Fr& sk,
              const std::pair<int, G1>& user_upd_info, const Ciphertext& ct);
