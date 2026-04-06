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
    ISOTP_FS_CTS,  // 继续
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
    ISOTP_RX_IDLE = 0,       

    // ISOTP_RX_SF,           
    
    ISOTP_RX_FF,         
    ISOTP_RX_FC,
     
    ISOTP_RX_CF,           
    
    ISOTP_RX_WAIT,
    
    // ISOTP_RX_DONE,            // 接收完成
    
    
} Isotp_RxState_t; // 接收状态机

typedef enum
{
    ISOTP_TX_IDLE = 0,
    ISOTP_TX_SF, // 发送单帧
    ISOTP_TX_FF, // 发送首帧
    ISOTP_TX_FC, // 接收流控帧
    ISOTP_TX_CF, // 发送连续帧
    // ISOTP_TX_DONE, // 完成
}Isotp_TxState_t; // 发送状态机

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
        uint16_t wait_tick;
        uint16_t wait_count;
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
    uint16_t total_len;
    uint16_t index_len;

    bool Isbusy; // 当前是否忙?

    struct {
        uint8_t data[8];
        uint8_t Bs;
        uint16_t STmin;
        uint8_t Fs;
    }FC;

    uint8_t SN; 
    uint8_t FcWait_count; // 流控等待次数
    uint32_t N_Bs; // N-bs计时器
    uint16_t cf_count; 

    uint32_t last_tick; 
}Isotp_tx_packet_t;

typedef struct 
{
    uint32_t tick; 
}Isotp_N_As_t; // N_AS定时器
typedef struct
{
    volatile uint32_t tick; // 心跳
    Isotp_Frame_t frame;
    Isotp_Transmit_t tx;
    Isotp_rx_packet_t rx_packet; // 接收数据包管理
    Isotp_tx_packet_t tx_packet; // 发送数据包管理
    Isotp_UplayerTrans_t Uplayer;
    Isotp_RxState_t rx_state;
    Isotp_TxState_t tx_state; 
} Isotp_Obj_t;
/* Exported constants --------------------------------------------------------*/

/* Exported macros -----------------------------------------------------------*/

/* Exported functions --------------------------------------------------------*/
extern void Isotp_Init(void);
#ifdef __cplusplus
}
#endif

#endif /* ISOTP_H_ */
