#ifndef MICROUDS_TYPES_H
#define MICROUDS_TYPES_H

#include "stdbool.h"
#include "string.h"
#include "stdlib.h"
#include "MicroUds_conf.h"
#include "MicroUds_utils.h"
#include "stdint.h"
#if MICROUDS_USE_MEM
#include "MicroMem.h"
#endif

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum
{
    MICROUDS_OK,
    MICROUDS_ERR,
} MicroUds_Status_t;

// typedef enum
// {

// }MicroUds_Secction_t; // 会话掩码

typedef struct
{
    uint8_t id;

    void(*func)(void); // 子功能回调函数
} MicroUds_Subfunc_t;

typedef struct
{
    uint16_t sec_mask;           // 服务掩码
    MicroUds_Subfunc_t *subfunc; // 子功能
} MicroUds_Servies_t;            // 服务

typedef struct {
    uint32_t seedkey; // 种子请求
    uint8_t level; // 当前安全等级
} MicroUds_Sericuty_t;

typedef struct {
    uint32_t tick;
    uint32_t timeout;
} MicroUds_Timer_t;

typedef struct
{
    MicroUds_Servies_t *servies; // 服务表
    MicroUds_Sericuty_t sericuty; // 安全
    volatile uint32_t tick; // 嘀嗒计数器

    MicroUds_Timer_t P2Server;
    MicroUds_Timer_t S3Server;
} MicroUds_Obj_t;

#ifdef __cplusplus
}
#endif

#endif
