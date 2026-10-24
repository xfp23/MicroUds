/**
 * @file MicroUdsServer_conf.h
 * @author xfp23
 * @brief MicroUds 配置项
 * @version \ref MICROUDS_VERSION
 * @date 2026-10-21
 *
 * @copyright Copyright (c) 2026
 *
 */
#ifndef MICROUDS_CONF_H
#define MICROUDS_CONF_H

#ifdef __cplusplus
extern "C"
{
#endif

#define MICROUDS_VERSION "0.0.2"

#define MICROUDS_SERVER_ENABLE 1u
#define MICROUDS_CLIENT_ENABLE 0u

// MICROUDS 频率 (单位: HZ) 与MicroUds_TickHandler(); 的调用频率保持一致
#define MICROUDS_FREQ_HZ 1000u

// [REVIEW] 会话保持时长(S3Server)，单位ms。原来直接写tick数(5000u)，现在由ms换算，名字 MICROUDS_SESSION_HOLD_TICK 保持不变
#define MICROUDS_SESSION_HOLD_MS 5000u
#define MICROUDS_SESSION_HOLD_TICK MICROUDS_MS_TO_TICK(MICROUDS_SESSION_HOLD_MS)

// [REVIEW] 0x10 正响应里要回给诊断仪的 P2Server / P2*Server
#define MICROUDS_P2_SERVER_MS 50u
#define MICROUDS_P2STAR_SERVER_MS 5000u

// [REVIEW] 服务返回PENDING后，每隔多久补发一次 0x78，必须小于 P2*Server，留出余量
#define MICROUDS_PENDING_RESEND_MS 4000u
// 最多发多少次 0x78，超过则内核终止请求并返回 0x10
#define MICROUDS_PENDING_MAX_COUNT 10u

#if (MICROUDS_PENDING_RESEND_MS >= MICROUDS_P2STAR_SERVER_MS)
#error "MICROUDS_PENDING_RESEND_MS must be smaller than MICROUDS_P2STAR_SERVER_MS"
#endif

// [REVIEW] 响应交给ISOTP之后，最多等多久收到 MicroUds_HwRespTxDone()。
//          超时就认为这条响应已经结束(下层丢了完成通知)，避免请求永远卡在"等发送完成"。要大于ISOTP自己的发送超时
#define MICROUDS_TX_CONFIRM_TIMEOUT_MS 1000u

#define MICROUDS_USE_MEM 0u

// [REVIEW] 响应缓冲，0x10 的正响应需要6字节，0x22读VIN需要 3+17，所以32勉强够用，给了64
#define MICROUDS_RESPONSE_BUF_SIZE 64u

// [REVIEW] 请求缓冲。RxIndication 返回后传输层的buffer可能被复用，而异步服务(0x78之后)还要读请求，所以内核自己拷贝一份
#define MICROUDS_REQUEST_BUF_SIZE 64u

// 支持的会话 [REVIEW] 含义是 "最大会话编号": 4 代表支持 10 01/02/03/04 ，3 代表支持 10 01/02/03
// 内核据此生成会话位掩码 (bit1..bitN)
#define MICROUDS_SESSION_MASK 4u

// 种子大小
#define MICROUDS_SEED_SIZE 4u

// 密钥大小
#define MICROUDS_KEY_SIZE 4u

// [REVIEW] 安全访问: 支持的安全等级数量 (等级1对应 27 01/02，等级2对应 27 03/04 ...)
#define MICROUDS_SECURITY_MAX_LEVEL 4u
// 连续失败多少次后锁定
#define MICROUDS_SECURITY_MAX_ATTEMPTS 3u
// 锁定后的延时(ms)，延时内请求种子返回 0x37
#define MICROUDS_SECURITY_DELAY_MS 10000u

// [REVIEW] 服务表容量，内核内置的 0x10/0x27/0x3E 占3个，DID模块再占 0x22/0x2E/0x2C 3个，所以比原来的10大一些
#define MICROUDS_SERVICE_MAX_NUMBER 16u

#define MICROUDS_SUBFUNC_MAX_NUMBER 3u

#if (MICROUDS_RESPONSE_BUF_SIZE < 8u)
#error "MICROUDS_RESPONSE_BUF_SIZE too small"
#endif

#define MICROUDS_DID_ENABLE 1u

#define MICROUDS_DID_READ_SIZE 8u

#define MICROUDS_DID_WRITE_SIZE 8u

// [REVIEW] 下面这两个原来被用作 0x22/0x2E 的"请求长度"上下限传给服务表，但名字叫 RESP(响应)，而且数值也不对:
//          0x22 请求最短是 3 字节(SID+DID)，0x2E 最短是 4 字节(SID+DID+至少1字节数据)，
//          0x2E 写一个 40 字节的DID，请求长度是 43，会被 max=32 挡成 0x13。
//          这两个宏现在不再使用，保留只是为了不破坏你的工程，请用下面新增的 REQ 系列宏。
#define MICROUDS_DID_RESP_MAX_LEN 32u

#define MICROUDS_DID_RESP_MIN_LEN 2u

#define MICROUDS_DID_DYNAMICALLY_SIZE 8u

#define MICROUDS_DID_DYNAMICALLY_ITEM_LEN 5u

/* ---------------- [REVIEW] 以下为 DID 模块新增配置 ---------------- */

// 0x22 / 0x2E / 0x2C 的完整请求长度下限(含SID)。上限不在服务表里限制(0x22 的DID个数超限要回 0x31 而不是 0x13，由回调判断)
#define MICROUDS_DID_READ_REQ_MIN_LEN 3u    // 22 + DID(2)
#define MICROUDS_DID_WRITE_REQ_MIN_LEN 4u   // 2E + DID(2) + 至少1字节数据
#define MICROUDS_DID_DYNA_REQ_MIN_LEN 2u    // 2C + 子功能(03 清除全部时只有这2字节)

// 0x22 一条请求最多带几个DID，超过返回 0x31
#define MICROUDS_DID_READ_MAX_PER_REQ 8u

// 多DID请求里有DID不存在/当前会话不允许时: 0=整条回负响应(严格，多数OEM要求); 1=跳过这个DID，至少有一个可读就返回
// (安全不满足 0x33 始终是整条拒绝，不会返回部分数据)
#define MICROUDS_DID_READ_PARTIAL 0u

// DID 在当前会话不可用时返回的NRC。你原来用的是 0x22(CNC)，常见规范是 0x31(ROOR)，所以改成宏，按你们OEM规范改
#define MICROUDS_DID_NRC_SESSION 0x31u

// 动态DID的合法范围。标准里 0xF200~0xF2FF 是周期DID(0x2A用)，0xF300~0xF3FF 才是动态定义DID；如果你们OEM约定用 F2xx，改这里
#define MICROUDS_DID_DYNA_ID_MIN 0xF300u
#define MICROUDS_DID_DYNA_ID_MAX 0xF3FFu

// 是否允许 0x2C 02 (按内存地址定义动态DID)。允许诊断仪读内存，量产默认关闭；打开时必须实现 MicroUds_Did_AddrReadable() 白名单
#define MICROUDS_DID_DYNA_BY_ADDR 1u

// 回到默认会话时是否清除所有动态DID (多数规范: 复位或回默认会话后动态定义失效)
#define MICROUDS_DID_DYNA_CLEAR_ON_DEFAULT 1u

#ifdef __cplusplus
}
#endif

#endif