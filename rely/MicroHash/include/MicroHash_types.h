/**
 * @file MicroHash_types.h
 * @author your name (you@domain.com)
 * @brief 哈希表类型
 * @version 0.1
 * @date 2025-10-28
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef MICROHASH_TYPES_H
#define MICROHASH_TYPES_H

#include "stdint.h"
#include "MicroHash_conf.h"

#ifdef __cplusplus
extern  "C"
{
#endif

typedef enum {
    MICROHASH_OK,
    MICROHASH_ERR,
    MICROHASH_ERR_PARAM,
    MICROHASH_ERR_MEMORY,
    MICROHASH_FULL

}MicroHash_Sta_t;

typedef struct MicroHash_Node_t
{
    MicroHash_key_t key; // 哈希值
    void *data;
    MicroHash_Node_t *next; // key冲突链表
}MicroHash_Node_t;

typedef struct
{
    size_t buckSize;
}MicroHash_Conf_t;

typedef struct {
    MicroHash_Node_t **buckets; // 桶
    MicroHash_Conf_t conf; // 配置
    size_t Remain; //剩余
}MicroHash_Obj;

typedef MicroHash_Obj *MicroHash_Handle_t; // 句柄

#ifdef __cplusplus
}
#endif


#endif //MICROHASH_TYPES_H