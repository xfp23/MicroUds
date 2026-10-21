/**
 * @file MicroUds.h
 * @author xfp23
 * @brief MicroUds
 * @version 0.1
 * @date 2026-10-21
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef MICROUDS_H
#define MICROUDS_H

#include "MicroUds_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief 
 * 
 * @return MicroUds_Status_t 
 */
extern MicroUds_Status_t MicroUds_Init(void);

/**
 * @brief 
 * 
 */
extern void MicroUds_TickHandler(void);

/**
 * @brief 
 * 
 */
extern void MicroUds_TimerHandler(void);

/**
 * @brief 
 * 
 * @param data 
 * @param len 
 */
extern void MicroUds_RxIndication(const uint8_t *data, size_t len, MicroUds_AddrType_t addrType);

/**
 * @brief 
 * 
 * @param level 
 * @param out 
 * @param len 
 * @return MicroUds_Status_t 
 */
extern void MicroUds_SecurityReqSeed(uint8_t level, uint8_t *out, uint32_t *len);

/**
 * @brief 
 * 
 * @param data 
 * @param len 
 * @return true 
 * @return false 
 */
extern void  MicroUds_SecurityGetKey(uint8_t *data,uint32_t *len);

#ifdef __cplusplus
}
#endif

#endif
