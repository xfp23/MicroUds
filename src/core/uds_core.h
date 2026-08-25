/**
 * @file    uds_core.h
 * @author  https://xfp23.github.io
 * @version 0.1
 * @date    2026/04/02
 */

#ifndef UDS_CORE_H_
#define UDS_CORE_H_

/* Includes ------------------------------------------------------------------*/
#include "MicroUds_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Exported types ------------------------------------------------------------*/
typedef struct 
{
    uint8_t rx_buf[MICROUDS_RX_BUFFER_MEMORY_SIZE]; // uds core  rx memory pool
    uint8_t tx_buf[MICROUDS_TX_BUFFER_MEMORY_SIZE]; // uds core tx memory pool
}Uds_Core_t; 
/* Exported constants --------------------------------------------------------*/
extern Uds_Core_t core_obj;
/* Exported macros -----------------------------------------------------------*/

/* Exported functions --------------------------------------------------------*/

#ifdef __cplusplus
}
#endif

#endif /* UDS_CORE_H_ */
