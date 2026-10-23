#ifndef MICROUDS_DID_H
#define MICROUDS_DID_H

#include "MicroUds_Did_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

extern MicroUds_Status_t MicroUds_Did_Init(void);

extern MicroUds_Status_t MicroUds_Did_RegisterReadItem(MicroUdsDidItem_Config_t *table, size_t size);

#ifdef __cplusplus
}
#endif

#endif
