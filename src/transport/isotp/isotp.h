/**
 * @file    isotp.h
 * @author  https://github.com/xfp23
 * @brief   ISO-15765-2
 * @version 0.1
 * @date    2026/04/02
 */

#ifndef ISOTP_H_
#define ISOTP_H_

/* Includes ------------------------------------------------------------------*/
#include "stdint.h"
#include "stdbool.h"
#include "isotp_conf.h"

#ifdef __cplusplus
extern "C" {
#endif
/* Exported types ------------------------------------------------------------*/
typedef struct {
    uint8_t data[8];
    bool en; // received single
    bool isPhySical;
}Isotp_Frame_t;

typedef struct {
    volatile uint32_t tick; // 心跳
    Isotp_Frame_t frame;
    uint8_t *buffer; // 数据buffer
}Isotp_Obj_t;
/* Exported constants --------------------------------------------------------*/

/* Exported macros -----------------------------------------------------------*/

/* Exported functions --------------------------------------------------------*/
extern void Isotp_Init(void);
#ifdef __cplusplus
}
#endif

#endif /* ISOTP_H_ */
