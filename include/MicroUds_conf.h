/**
 * @file MicroUds_conf.h
 * @author xfp23
 * @brief MicroUds 配置项
 * @version 0.2
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

// MICROUDS 频率 (单位: HZ) 与MicroUds_TickHandler(); 的调用频率保持一致
#define MICROUDS_FREQ_HZ 1000u

// [REVIEW] 毫秒转tick(向上取整)。所有时间配置项统一用ms书写，避免 "改了频率忘改tick数" 的坑。
// 注意: ms * FREQ_HZ 用32位计算，ms 不要超过 4294967 (FREQ=1000时约71分钟)
#define MICROUDS_MS_TO_TICK(ms) ((((uint32_t)(ms)) * MICROUDS_FREQ_HZ + 999u) / 1000u)

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

// [REVIEW] 服务表容量，内核内置的 0x10/0x27/0x3E 占3个，所以比原来的10大一些
#define MICROUDS_SERVICE_MAX_NUMBER 16u

#define MICROUDS_SUBFUNC_MAX_NUMBER 3u

#if (MICROUDS_RESPONSE_BUF_SIZE < 8u)
#error "MICROUDS_RESPONSE_BUF_SIZE too small"
#endif

#define MICROUDS_DID_ENABLE 1u

#define MICROUDS_DID_READ_SIZE 8u

#define MICROUDS_DID_WRITE_SIZE 8u

#define MICROUDS_DID_RESP_MAX_LEN 32u

#define MICROUDS_DID_RESP_MIN_LEN 2u

#ifdef __cplusplus
}
#endif

#endif