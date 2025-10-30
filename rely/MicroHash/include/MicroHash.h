/**
 * @file MicroHash.h
 * @author xfp23
 * @brief 哈希表
 * @version 0.1
 * @date 2025-10-30
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#ifndef MICROHASH_H
#define MICROHASH_H

#include "MicroHash_types.h"
#include "math.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief 初始化哈希表
 *
 *
 * @param handle 句柄地址，必须是NULL的句柄
 * @param conf 配置
 * @return MicroHash_Sta_t
 */
extern MicroHash_Sta_t MicroHash_Init(MicroHash_Handle_t *handle, MicroHash_Conf_t *conf);

/**
 * @brief 插入数据
 *
 * @param handle 句柄
 * @param key 键
 * @param data 用户数据
 * @return MicroHash_Sta_t 状态
 */
extern MicroHash_Sta_t MicroHash_Insert(MicroHash_Handle_t *handle, MicroHash_key_t key, void *data);

/**
 * @brief 查找数据
 *
 * @param handle 句柄
 * @param key 键
 * @return void* 查找到的数据
 */
extern void *MicroHash_Find(MicroHash_Handle_t *handle, MicroHash_key_t key);

/**
 * @brief 移除数据
 *
 * @param handle 句柄
 * @param key 键
 * @return MicroHash_Sta_t 状态
 */
extern MicroHash_Sta_t MicroHash_Remove(MicroHash_Handle_t *handle, MicroHash_key_t key);

/**
 * @brief 删除哈希表，释放所占用的资源
 *
 * @param handle 句柄
 * @return MicroHash_Sta_t 状态
 */
extern MicroHash_Sta_t MicroHash_Delete(MicroHash_Handle_t *handle);

#ifdef __cplusplus
}
#endif

#endif