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
 * @brief 传输层 ISOTP-DOCAN
 * 
 */
#define MICROUDS_TRANSPORT_ISOTP_DOCAN

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
#define MICROUDS_N_CR (3000u)

/**
 * @brief N_Wait 流控帧等待次数
 * 
 */
#define MICROUDS_FC_WAIT_MAX (10u)

/**
 * @brief 作为接收方等待wait发送时间间隔
 * 
 * 单位 : ms
 * 
 */
#define MICROUDS_FC_WAITTIME (100u) 

/**
 * @brief N_Ar定时器
 *  单位 : ms
 * 
 */
#define MICROUDS_N_AR (10000)




#ifdef __cplusplus
}
#endif

#endif /* MICROUDS_CONF_H_ */
