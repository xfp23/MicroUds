/**
 * @file    isotp_conf.h
 * @author  https://github.com/xfp23
 * @version 0.1
 * @date    2026/04/02
 */

#ifndef ISOTP_CONF_H_
#define ISOTP_CONF_H_


/* Includes ------------------------------------------------------------------*/
#include "MicroUds_conf.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief UDS buffer Memory byte len
 * 
 */
#ifndef MICROUDS_RX_BUFFER_MEMORY_SIZE
#define MICROUDS_RX_BUFFER_MEMORY_SIZE (4150u)
#endif

#ifdef __cplusplus
}
#endif

#endif /* ISOTP_CONF_H_ */
