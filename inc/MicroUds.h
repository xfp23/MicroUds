/**
 * @file    MicroUds.h
 * @author  https://xfp23.github.io
 * @version 0.1
 * @date    2026/04/02
 */

#ifndef MICROUDS_H_
#define MICROUDS_H_

/* Includes ------------------------------------------------------------------*/

#ifdef __cplusplus
extern "C" {
#endif

/* Exported types ------------------------------------------------------------*/

/* Exported constants --------------------------------------------------------*/

/* Exported macros -----------------------------------------------------------*/

/* Exported functions --------------------------------------------------------*/

/**
 * @brief  初始化 MicroUds
 * 
 */
extern void MicroUds_Init(void); 

/**
 * @brief 滴答定时器
 * 
 */
extern void MicroUds_TickHandler(void);

/**
 * @brief 主循环1ms回调
 * 
 */
extern void MicroUds_TimerHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* MICROUDS_H_ */
