/**
 * @file    uds_can_port.h
 * @author  https://xfp23.github.io
 * @version 0.1
 * @date    2026/04/02
 */

#ifndef UDS_TRANS_PORT_H_
#define UDS_TRANS_PORT_H_

/* Includes ------------------------------------------------------------------*/
#include "MicroUds_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Exported types ------------------------------------------------------------*/

/* Exported constants --------------------------------------------------------*/

/* Exported macros -----------------------------------------------------------*/

/* Exported functions --------------------------------------------------------*/

extern int MicroUds_Trans_ResponseAddress(uint8_t *data, size_t len);

extern void MicroUds_Trans_PhysicalAddress(uint8_t *data, size_t len);

extern void MicroUds_Trans_FunctionAddress(uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* UDS_TRANS_PORT_H_ */
