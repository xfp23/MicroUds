
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// 哈希乘法常数 (来自您之前的优化): 1101524993u
#define HASH_CONSTANT 1101524993U

/**
 * @brief 计算哈希值
 * * 算法: (key * HASH_CONSTANT) % buckSize
 * * @param key 输入的键值 (uint32_t)
 * @param buckSize 桶的大小 (M)
 * @return uint32_t 哈希结果 (0 到 buckSize - 1)
 */
uint32_t calculate_hash(uint32_t key, uint32_t buckSize) {
    if (buckSize == 0) {
        // 避免除以零
        return 0;
    }
    // 注意：C语言标准保证 uint32_t * uint32_t 的结果是 uint32_t，
    // 自动截断到 32 位，但在这里，我们使用 uint64_t 确保乘积不会溢出，
    // 以获得最高质量的哈希结果。
    uint64_t product = (uint64_t)key * HASH_CONSTANT;

    // 取模操作
    return (uint32_t)(product % buckSize);
}

// ====================================================================

int main() {
    uint32_t buckSize;
    uint32_t startKey;
    uint32_t endKey;

    printf("--- 哈希冲突统计程序 ---\n");
    printf("哈希函数: (key * 1101524993u) %% buckSize\n\n");

    // 1. 获取用户输入
    printf("请输入桶的大小 (buckSize, M): ");
    if (scanf("%u", &buckSize) != 1 || buckSize == 0) {
        printf("错误: 桶大小必须是大于0的整数。\n");
        return 1;
    }

    printf("请输入 key 的起始值 (Start Key, >= 1): ");
    if (scanf("%u", &startKey) != 1 || startKey == 0) {
        printf("错误: Key 的起始值必须大于等于 1。\n");
        return 1;
    }

    printf("请输入 key 的结束值 (End Key, >= Start Key): ");
    if (scanf("%u", &endKey) != 1 || endKey < startKey) {
        printf("错误: Key 的结束值必须大于等于起始值。\n");
        return 1;
    }

    // 2. 准备统计变量
    // 使用一个数组来标记哪些哈希值已经被使用
    // 注意：如果 buckSize 很大，这个数组会占用大量内存。
    // 为了简化，我们只支持 buckSize 小于等于 1024 的情况。
    // 如果 buckSize 很大，应该使用哈希表或动态分配。
    #define MAX_BUCK_SIZE 1024
    if (buckSize > MAX_BUCK_SIZE) {
         printf("注意: 桶大小 %u 过大，程序已限制为 <= %d。请使用较小的值测试。\n", buckSize, MAX_BUCK_SIZE);
         return 1;
    }

    bool is_hashed[MAX_BUCK_SIZE] = {false};
    uint32_t distinct_hashes = 0;
    uint32_t total_inputs = 0;

    printf("\n--- 计算结果 ---\n");
    printf("Key 范围: [%u, %u], 桶大小: %u\n", startKey, endKey, buckSize);
    printf("----------------\n");

    // 3. 循环计算并统计
    for (uint32_t key = startKey; key <= endKey; ++key) {
        uint32_t hash_result = calculate_hash(key, buckSize);
        total_inputs++;

        // 打印结果 (每行最多 4 个)
        printf("Hash(%u) = %u", key, hash_result);
        if (total_inputs % 4 == 0) {
            printf("\n");
        } else {
            printf("\t");
        }

        // 统计不同的哈希值
        if (!is_hashed[hash_result]) {
            is_hashed[hash_result] = true;
            distinct_hashes++;
        }

        // 处理 key 溢出 (uint32_t最大值)
        if (key == 0xFFFFFFFF) break;
    }
    printf("\n----------------\n");

    // 4. 输出统计结果
    uint32_t total_collisions = total_inputs - distinct_hashes;

    printf("\n--- 统计分析 ---\n");
    printf("* 总输入 Key 数量: %u\n", total_inputs);
    printf("* 不同的哈希值数量: %u\n", distinct_hashes);
    printf("* 哈希桶总数量: %u\n", buckSize);

    // 如果不同的哈希值数量小于输入数量，则一定发生了冲突
    if (total_collisions > 0) {
        printf("🚨 **总冲突次数: %u**\n", total_collisions);
        printf("说明: 输入 Key 的数量大于不同的哈希值数量，因此发生了重复映射。\n");
    } else {
        printf("✅ **总冲突次数: 0**\n");
        if (distinct_hashes == buckSize && total_inputs >= buckSize) {
             printf("说明: 所有的桶都被至少使用了一次 (完美覆盖)。\n");
        } else if (distinct_hashes < buckSize) {
             printf("说明: 没有冲突，但部分哈希桶没有被使用。\n");
        }
    }

    return 0;
}
