/**
 * @file MicroUdsServer.h
 * @author xfp23
 * @brief MicroUdsServer uds服务器实现
 * @version \ref MICROUDS_VERSION
 * @date 2026-10-21
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef MICROUDS_H
#define MICROUDS_H

#include "MicroUdsServer_types.h"
#include "MicroUdsServer_Did.h"

#ifdef __cplusplus
extern "C"
{
#endif

#if MICROUDS_SERVER_ENABLE

/* ============================== 内核API ============================== */

/**
 * @brief 初始化。会同时注册内核内置服务 0x10 / 0x27 / 0x3E，这三个SID不能再被用户注册
 * 
 * @return MicroUds_Status_t 
 */
extern MicroUds_Status_t MicroUdsServer_Init(void);

/**
 * @brief 心跳，频率与 MICROUDS_FREQ_HZ 一致，可在定时中断里调用
 * 
 */
extern void MicroUdsServer_TickHandler(void);

/**
 * @brief 主循环函数，在主循环/任务中周期调用(周期不应大于 1 tick 太多)。
 *        负责会话超时(S3)、服务分发、PENDING 重入、响应发送。
 * 
 */
extern void MicroUdsServer_TimerHandler(void);

/**
 * @brief 输入一条完整的请求PDU (由传输层 MicroIsotp 调用)
 * 
 * @param data 完整PDU，含SID。内核会拷贝，函数返回后data可被复用
 * @param len  长度
 * @param addrType 物理/功能寻址
 * @return MICROUDS_OK 已接收；MICROUDS_BUSY 上一条请求还没处理完，本条被丢弃(传输层可选择重试)；
 *         MICROUDS_PARAM_ERR 参数非法(空指针或长度0，没有SID也就无法构造任何响应，直接丢弃)
 * @note  [REVIEW] 原来是 void。返回值让传输层知道请求有没有被接收。
 */
extern MicroUds_Status_t MicroUdsServer_RxIndication(const uint8_t *data, size_t len, MicroUds_AddrType_t addrType);

/* ============================== 服务注册 ============================== */
// [REVIEW] 原头文件里漏了这两个函数的声明

/**
 * @brief 注册服务(追加)。整张表要么全部成功，要么全部不注册。SID重复会失败
 */
extern MicroUds_Status_t MicroUdsServer_ServiceRegisterServ(const MicroUdsServer_ServiceTable_t *table, size_t size);

/**
 * @brief 为已注册的、带 HAS_SUBFUNC 标志的服务注册子功能表(替换该服务原有的子功能表)
 */
extern MicroUds_Status_t MicroUdsServer_ServiceRegisterSubFunc(uint8_t sid, const MicroUdsServer_SubFuncTable_t *table, size_t size);

/* ============================== 状态查询 ============================== */

extern uint8_t MicroUdsServer_GetSession(void);       // 当前会话编号
extern uint8_t MicroUdsServer_GetSecurityLevel(void); // 当前已解锁的安全等级，0=未解锁

/* ============================== 回调里使用的辅助函数 ============================== */

/** 回调成功: 响应数据已写入 ctx->payload，共 len 字节 (不含 RSID/子功能，内核会补) */
static inline void MicroUdsServer_CtxPositive(MicroUdsServer_ServiceCtx_t *ctx, uint32_t len)
{
    ctx->resp_type = MICROUDS_RESPTYPE_POSITIVE;
    ctx->len = len;
}

/** 回调失败: 返回负响应 */
static inline void MicroUdsServer_CtxNegative(MicroUdsServer_ServiceCtx_t *ctx, uint8_t nrc)
{
    ctx->resp_type = MICROUDS_RESPTYPE_NEGATIVE;
    ctx->nrc = nrc;
}

/** 回调未完成: 内核发 0x78，之后每个 TimerHandler 周期再次调用本回调(ctx->call_cnt > 0)。回调必须非阻塞 */
static inline void MicroUdsServer_CtxPending(MicroUdsServer_ServiceCtx_t *ctx)
{
    ctx->resp_type = MICROUDS_RESPTYPE_PENDING;
}

/* ============================== 用户需要实现的弱函数 ============================== */

/**
 * @brief 发送一帧完整响应PDU (弱函数，用户重写，一般转给 MicroIsotp)
 * 
 * @return MICROUDS_OK 已接受发送；MICROUDS_BUSY 传输层暂时忙，内核下个周期重试；其他 丢弃本条响应
 * @note   目前没有"发送完成确认"，0x10的会话切换在 Transmit 返回OK时就生效。
 *         后续建议增加 MicroUdsServer_TxConfirmation() 让传输层在真正发完后通知内核。
 */
extern MicroUds_Status_t MicroUdsServer_Transmit(const uint8_t *pdu, uint32_t len);

/**
 * @brief 响应完成通知,由ISOTP响应发送完成后回调
 * 
 */
extern void MicroUdsServer_HwRespTxDone(void);

/**
 * @brief 生成种子 (弱函数，用户重写)
 * 
 * @param level 安全等级 (27 01 -> 1，27 03 -> 2 ...)
 * @param out   种子输出缓冲
 * @param len   [in] out 的容量(=MICROUDS_SEED_SIZE)；[out] 实际生成的种子长度，设为0表示生成失败
 */
extern void MicroUdsServer_SecurityReqSeed(uint8_t level, uint8_t *out, uint32_t *len);

/**
 * @brief 根据种子计算期望的密钥 (弱函数，用户重写)
 * @note  [REVIEW] 原来的签名 GetKey(key, len) 没有种子和等级入参，根本没法算密钥，所以加上了
 *        默认实现返回 *key_len=0，表示"不支持"，此时任何密钥都校验失败(安全的默认值)
 * 
 * @param level    安全等级
 * @param seed     刚才发给诊断仪的种子
 * @param seed_len 种子长度
 * @param key      期望密钥输出缓冲
 * @param key_len  [in] key 的容量(=MICROUDS_KEY_SIZE)；[out] 实际密钥长度
 */
extern void MicroUdsServer_SecurityGetKey(uint8_t level, const uint8_t *seed, uint32_t seed_len, uint8_t *key, uint32_t *key_len);

/**
 * @brief 内核内置服务回调 用户重写 比如 10 02 跳转bootloader
 * 
 * @param ctx 
 */
extern void MicroUdsServer_OnResponseSent(MicroUdsServer_ServiceCtx_t* ctx);

#endif

#ifdef __cplusplus
}
#endif

#endif