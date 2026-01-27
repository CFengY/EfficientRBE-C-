#pragma once
#include <RBE_Common.h>

// 辅助函数：将 G1 转换为 std::string (二进制数据)
inline std::string g1_to_bin(const G1& p) {
    std::string s;
    // mcl::IoSerialize 模式会将数据压缩为紧凑的二进制格式
    p.getStr(s, mcl::IoSerialize);
    return s;
}

// 辅助函数：将 std::string (二进制数据) 还原为 G1
inline G1 bin_to_g1(const std::string& s) {
    G1 p;
    if (s.empty()) {
        p.clear();  // 视为空串为无穷远点（零元）
    } else {
        p.setStr(s, mcl::IoSerialize);
    }
    return p;
}