#ifndef MICROUDS_H
#define MICROUDS_H

/**
 * @file Microuds.h
 * @author https://github.com/xfp23
 * @brief 
 * @version 0.1
 * @date 2025-10-21
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include "Microuds_types.h"
#include "Isotp.h"

#ifdef __cplusplus
extern "C"
{
#endif

extern MicroUDS_Sta_t MicroUDS_Init(MicroHash_Conf_t *conf);

// extern MicroUDS_Sta_t MicroUDS_RegisterService(MicroUDS_Service_t sid,Microuds_RegisterTable_t *table,uint8_t sslen);

extern void MicroUDS_TimerHandler(void);


#ifdef __cplusplus
}
#endif

#endif