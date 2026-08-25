/**
 * @file    uds_dispatch.h
 * @author  https://xfp23.github.io
 * @version 0.1
 * @date    2026/04/02
 */

#ifndef UDS_DISPATCH_H_
#define UDS_DISPATCH_H_

/* Includes ------------------------------------------------------------------*/
#include "MicroUds_types.h"
#include "uds_context.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Exported types ------------------------------------------------------------*/
typedef struct
{
    uint8_t sid;
    // uint16_t sub_function; 
    // Uds_SubFunc_t sub_func;
    Uds_SerivceFunc_t service;

} Uds_Dispatch_Obj_t;
/* Exported constants --------------------------------------------------------*/

/* Exported macros -----------------------------------------------------------*/
extern Uds_Dispatch_Obj_t uds_dispatch_obj;
/* Exported functions --------------------------------------------------------*/
extern void uds_dispatch_timerhandler(void);

#ifdef __cplusplus
}
#endif

#endif /* UDS_DISPATCH_H_ */
