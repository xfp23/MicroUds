/**
 * @file MicroUdsServer_Did.h
 * @author Xfp23
 * @brief DID 模块 (0x22 / 0x2E / 0x2C)
 * @version 0.1
 * @date 2026-10-23
 *
 * @copyright Copyright (c) 2026
 *
 */
#ifndef MICROUDS_DID_H
#define MICROUDS_DID_H

#include "MicroUdsServer_Did_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

#if MICROUDS_DID_ENABLE

/**
 * @brief 初始化 DID 模块 (由 MicroUdsServer_Init 调用，用户不需要自己调)。
 *        所以用户的 DID 表必须在 MicroUdsServer_Init 之后注册。
 */
extern MicroUdsServer_Status_t MicroUdsServer_Did_Init(void);

/**
 * @brief 注册可读DID。可多次调用，后面的追加在前面的后面。整张表要么全部成功要么全部不注册
 * [REVIEW] 返回 PARAM_ERR 的情况: 空表、超过容量、DID重复、data_len为0或超过响应缓冲、读表项 data 为空
 */
extern MicroUdsServer_Status_t MicroUdsServer_Did_RegisterReadItem(const MicroUdsServerDidItem_Config_t *table, size_t size);

/**
 * @brief 注册可写DID (不在写表里的DID就是只读)。规则同上；写表项 data 和 cb 至少要有一个
 */
extern MicroUdsServer_Status_t MicroUdsServer_Did_RegisterWriteItem(const MicroUdsServerDidItem_Config_t *table, size_t size);

/**
 * @brief 查找动态DID，未定义返回 NULL
 */
extern MicroUdsServerDid_Dynamically_t *MicroUdsServer_Did_FindDynamic(uint16_t id);

/* ---- 内核调用的服务回调 ---- */
extern void MicroUdsServer_Did_0x22ReadHandler(MicroUdsServer_ServiceCtx_t *ctx);
extern void MicroUdsServer_Did_0x2EWriteHandler(MicroUdsServer_ServiceCtx_t *ctx);
extern void MicroUdsServer_Did_0x2CDynamicHandler(MicroUdsServer_ServiceCtx_t *ctx);

/**
 * @brief 内核在会话发生变化后通知 DID 模块 (动态DID在回到默认会话时清除，见 MICROUDS_DID_DYNA_CLEAR_ON_DEFAULT)
 */
extern void MicroUdsServer_Did_OnSessionChange(uint8_t new_ssid);

/**
 * @brief 弱函数，用户重写: [addr, addr+len) 是否允许被 0x2C 02 读取。默认返回 false(全部拒绝)
 *        只有 MICROUDS_DID_DYNA_BY_ADDR=1 时才会被调用。请只放行明确列出的RAM/Flash区间。
 */
extern bool MicroUdsServer_Did_AddrReadable(uint32_t addr, uint32_t len);

#endif

#ifdef __cplusplus
}
#endif

#endif