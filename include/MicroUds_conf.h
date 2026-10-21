/**
 * @file MicroUds_conf.h
 * @author xfp23
 * @brief 
 * @version 0.1
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

#define MICROUDS_VERSION "0.0.1"

// MICROUDS 频率 (单位: HZ) 与MicroUds_TickHandler(); 的调用频率保持一致
#define MICROUDS_FREQ_HZ 1000u

// 会话保持时长
#define MICROUDS_SESSION_HOLD_TICK 5000u

#define MICROUDS_USE_MEM 0u

#define MICROUDS_RESPONSE_BUF_SIZE 32u

// 支持的会话 1 代表 10 01 2 代表 10 01/02 ....
#define MICROUDS_SESSION_MASK 4u

// 种子大小
#define MICROUDS_SEED_SIZE 4u

// 密钥大小
#define MICROUDS_KEY_SIZE 4u

#define MICROUDS_SERVICE_MAX_NUMBER 10u

#define MICROUDS_SUBFUNC_MAX_NUMBER 3u

#ifdef __cplusplus
}
#endif

#endif
