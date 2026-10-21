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

#define MICROUDS_USE_MEM 0u

#define MICROUDS_RESPONSE_BUF_SIZE 32u

// 支持的会话 1 代表 10 01 2 代表 10 01/02 ....
#define MICROUDS_SESSION_MASK 4u

// 种子大小
#define MICROUDS_SEED_SIZE 4u

// 密钥大小
#define MICROUDS_KEY_SIZE 4u

#ifdef __cplusplus
}
#endif

#endif
