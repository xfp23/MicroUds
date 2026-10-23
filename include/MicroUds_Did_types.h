#ifndef MICROUDS_DID_TYPES_H
#define MICROUDS_DID_TYPES_H

#include "MicroUds_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct
{
    uint16_t Did;     // 标识符
    uint16_t Did_len; // did数量

    uint8_t *data;     // 数据
    uint16_t data_len; // 数据长度

    uint8_t nrc; // 用户输出

    void *Userdata; // 用户数据

} MicroUdsDid_Ctx_t;

typedef void (*MicroUdsDid_Cb_t)(MicroUdsDid_Ctx_t *);

typedef struct
{
    uint16_t Identifier; // 标识符

    // uint8_t flags; // 标志 似乎不需要标志

    uint8_t permis;  // 安全权限 0 直接放行
    uint8_t session; // 会话 0 直接放行

    uint8_t *data;     // 数据钩子，配置时指向，用于直接写入和读取用户的数据
    uint16_t data_len; // 数据长度

    volatile bool is_vaild; // 有效

    void *Userdata; // 用户数据
    MicroUdsDid_Cb_t cb;
} MicroUdsDidItem_t;

typedef struct
{
    uint16_t Identifier; // 标识符

    // uint8_t flags; // 标志

    uint8_t permis;  // 安全权限 0 直接放行
    uint8_t session; // 会话 0 直接放行

    uint8_t *data;     // 数据，配置时指向
    uint16_t data_len; // 数据长度

    void *Userdata; // 用户数据
    MicroUdsDid_Cb_t cb;
} MicroUdsDidItem_Config_t; // 配置列表

typedef struct
{
    MicroUdsDidItem_t Read[MICROUDS_DID_READ_SIZE];
    MicroUdsDidItem_t Write[MICROUDS_DID_WRITE_SIZE];

    uint16_t read_num;
    uint16_t write_num;

} MicroUdsDid_Obj_t;

#ifdef __cplusplus
}
#endif

#endif
