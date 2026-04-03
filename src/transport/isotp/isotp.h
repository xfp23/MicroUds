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
extern "C"
{
#endif
/* Exported types ------------------------------------------------------------*/

typedef enum
{
    ISOTP_TYPE_SF,
    ISOTP_TYPE_FF,
    ISOTP_TYPE_CF,
    ISOTP_TYPE_FC,
} Isotp_FrameType_t;

typedef enum
{
    ISOTP_FS_CON,  // 继续
    ISOTP_FS_WAIT, // 等待
    ISOTP_FS_OVER, // 溢出
} Isotp_FlowStatus_t;

typedef enum
{
    ISOTP_OK,
    ISOTP_ERR,
    ISOTP_BUSY,
}Isotp_Status_t;

typedef enum
{
    ISOTP_RX_IDLE = 0,        // 空闲，等待新帧

    // ISOTP_RX_SF,              // 收到单帧（直接完成）
    
    ISOTP_RX_FF,              // 收到首帧（已发送FC，准备接收CF）
    
    ISOTP_RX_CF,              // 正在接收连续帧
    
    ISOTP_RX_WAIT,            // （可选）内部等待资源（很少用）
    
    // ISOTP_RX_DONE,            // 接收完成
    
    ISOTP_RX_ERROR            // 接收错误（SN错/超时等）
    
} Isotp_RxState_t;

typedef struct
{
    uint16_t total_len;
    uint8_t *buffer;    // 数据buffer
    uint16_t index_len; // 数据索引

    struct
    {
        uint8_t STmin;                  // cf之间的间隔
        uint16_t Bs;                    // 数据块连续多少帧
        Isotp_FlowStatus_t Flow_status; // 流控状态
        bool isWaitFs;                  // 是否等待
        uint8_t Fc_payload[5];          // 流控帧负载
    } FC;

    struct 
    {
        uint8_t SN;
        uint32_t timeout;
        
    } CF; // 连续帧 
} Isotp_rx_packet_t;

typedef struct
{
    uint8_t data[8];
    bool en; // received single
    bool isPhySical;
    uint8_t len;

    Isotp_FrameType_t pci;
} Isotp_Frame_t;

typedef struct
{
    uint8_t data[8];
    uint8_t len;
    uint8_t en;
} Isotp_Transmit_t;

typedef struct
{
    // struct
    // {
    //     bool en;
    //     size_t len;
    //     // uint8_t *data;
    // } rx; // 从上层接收到数据

    struct
    {
        bool en;
        size_t len;
        // uint8_t *data;
    } tx; // 给上层发送数据

} Isotp_UplayerTrans_t; // 上层传输数据接口


typedef struct 
{
    uint8_t *buffer; // 发送数据缓冲区
    size_t total_len;
    size_t index_len;

    bool Isbusy; // 当前是否忙?
}Isotp_tx_packet_t;

typedef struct
{
    volatile uint32_t tick; // 心跳
    Isotp_Frame_t frame;
    Isotp_Transmit_t tx;
    Isotp_rx_packet_t rx_packet; // 接收数据包管理
    Isotp_tx_packet_t tx_packet; // 发送数据包管理
    Isotp_UplayerTrans_t Uplayer;
    Isotp_RxState_t rx_state;
    Isotp_RxState_t tx_state; 
} Isotp_Obj_t;
/* Exported constants --------------------------------------------------------*/

/* Exported macros -----------------------------------------------------------*/

/* Exported functions --------------------------------------------------------*/
extern void Isotp_Init(void);
#ifdef __cplusplus
}
#endif

#endif /* ISOTP_H_ */
