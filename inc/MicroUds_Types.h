/**
 * @file    MicroUds_types.h
 * @author  https://github.com/xfp23
 * @version 0.1
 * @date    2026/04/02
 */

#ifndef MICROUDS_TYPES_H_
#define MICROUDS_TYPES_H_

/* Includes ------------------------------------------------------------------*/
#include "MicroUds_conf.h"
#include "stdlib.h"
#include "stdint.h"
#include "stdbool.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Exported types ------------------------------------------------------------*/
typedef enum 
{
    MICROUDS_OK,
    MICROUDS_ERR,
    MICROUDS_FULL,
    MICROUDS_NOT_FOUND,
    MICROUDS_BUFF_OVERFLOW, // 缓冲区溢出
    MICROUDS_BUSY,
}MicroUds_Status_t;
/* Exported constants --------------------------------------------------------*/

/* Exported macros -----------------------------------------------------------*/

/* Exported functions --------------------------------------------------------*/

#ifdef __cplusplus
}
#endif

#endif /* MICROUDS_TYPES_H_ */
