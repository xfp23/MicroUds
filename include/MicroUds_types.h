#ifndef MICROUDS_TYPES_H
#define MICROUDS_TYPES_H

#include "stdbool.h"
#include "string.h"
#include "stdlib.h"
#include "MicroUds_conf.h"
#include "MicroUds_utils.h"
#if MICROUDS_USE_MEM
#include "MicroMem.h"
#endif

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum {
    MICROUDS_OK,
    MICROUDS_ERR,
}MicroUds_Status_t;



#ifdef __cplusplus
}
#endif

#endif
