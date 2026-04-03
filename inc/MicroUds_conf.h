/**
 * @file    MicroUds_conf.h
 * @author  https://github.com/xfp23
 * @version 0.1
 * @date    2026/04/02
 */

#ifndef MICROUDS_CONF_H_
#define MICROUDS_CONF_H_

/* Includes ------------------------------------------------------------------*/

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 接收数据缓冲区
 * 
 */
#define MICROUDS_RX_BUFFER_MEMORY_SIZE (4096U)

/**
 * @brief 发送数据缓冲区
 * 
 */
#define MICROUDS_TX_BUFFER_MEMORY_SIZE (128u)

/**
 * @brief 数据块大小 BS
 * 
 */
#define MICROUDS_BLOCK_SIZE (0u)

/**
 * @brief CF 间隔时间 STMIN
 * 
 */
#define MICROUDS_SEPARATION_TIME (127u)

/**
 * @brief FC填充字节
 * 
 */
#define MICROUDS_PADDING_BYTES (0x00)

/**
 * @brief N_BS 
 * 
 */
#define MICROUDS_N_BS (1000u)

/**
 * @brief N_CR
 * 
 */
#define MICROUDS_N_CR (1000u)

#ifdef __cplusplus
}
#endif

#endif /* MICROUDS_CONF_H_ */
