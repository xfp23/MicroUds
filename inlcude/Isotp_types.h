#ifndef ISOTP_TYPES_H
#define ISOTP_TYPES_H

/**
 * @file Isotp_types.h
 * @author
 * @brief
 * @version 0.1
 * @date 2025-10-22
 *
 * @copyright Copyright (c) 2025
 *
 */
#include "stdint.h"
#include "stdbool.h"
#include "stdlib.h"
#include "Isotp_conf.h"
#include "string.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum
{
    ISOTP_OK = 0,
    ISOTP_ERR_PARAM,
    ISOTP_ERR_LENGTH,
    ISOTP_ERR_TYPE,
    ISOTP_ERR_FRAME,
} Isotp_Sta_t;

typedef enum
{
    FRAME_SINGLE = 0x00,      // 单帧 
    FRAME_FIRST = 0x01,       // 首帧
    FRAME_CONSECUTIVE = 0x02, // 连续帧
    FRAME_FLOWCONTROL = 0x03, // 流控帧
} Isotp_FrameType_t;

typedef enum
{
    ISOTP_FS_CTS = 0x00,  /**< Continue To Send: 允许继续发送连续帧 */
    ISOTP_FS_WAIT = 0x01, /**< Wait: 暂停发送，等待下一个流控帧 */
    ISOTP_FS_OVFLW = 0x02 /**< Overflow/Abort: 缓冲区溢出，终止传输 */
} Isotp_FlowStatus_t;     /**< ISO-TP 流控状态枚举 */

/*------------------------------------------
  ISO-TP Single Frame
-------------------------------------------*/
typedef union
{
    struct
    {
#if ISOTP_BITFIELD_LITTLE_ENDIAN
        uint8_t PCI_DL : 4;  // 低 4 位：数据长度
        uint8_t PCIType : 4; // 高 4 位：帧类型
#elif ISOTP_BITFIELD_BIG_ENDIAN
        uint8_t PCIType : 4; // 高 4 位：帧类型
        uint8_t PCI_DL : 4;  // 低 4 位：数据长度
#endif
        uint8_t Payload[7]; // 数据区
    } byte;
    uint8_t data[8];
} Isotp_SingleFrame_t;

/*------------------------------------------
  ISO-TP First Frame
-------------------------------------------*/
typedef union
{
    struct
    {
#if ISOTP_BITFIELD_LITTLE_ENDIAN
        uint8_t FF_DL_H : 4;  // 高 4 位：消息长度高 4 位
        uint8_t PCIType : 4;  // 低 4 位：帧类型 = 1
#elif ISOTP_BITFIELD_BIG_ENDIAN
        uint8_t PCIType : 4;  // 高 4 位：帧类型 = 1
        uint8_t FF_DL_H : 4;  // 低 4 位：消息长度高 4 位
#endif
        uint8_t FF_DL_L;        // 消息长度低 8 位
        uint8_t Payload[6];     // 数据段
    } byte;
    uint8_t data[8];
} Isotp_FirstFrame_t;

/*------------------------------------------
  ISO-TP Flow Control Frame
-------------------------------------------*/
typedef union
{
    struct
    {
#if ISOTP_BITFIELD_LITTLE_ENDIAN
        uint8_t FS : 4;       // FlowStatus: 0=CTS, 1=WAIT, 2=OVFLW
        uint8_t PCIType : 4;  // 帧类型 = 3
#elif ISOTP_BITFIELD_BIG_ENDIAN
        uint8_t PCIType : 4;  // 帧类型 = 3
        uint8_t FS : 4;       // FlowStatus
#endif
        uint8_t BS;             // Block size
        uint8_t STmin;          // 最小间隔时间
        uint8_t Reserve[5];     // 保留
    } byte;
    uint8_t data[8];
} Isotp_FlowControlFrame_t;

/*------------------------------------------
  ISO-TP Consecutive Frame
-------------------------------------------*/
typedef union
{
    struct
    {
#if ISOTP_BITFIELD_LITTLE_ENDIAN
        uint8_t SN : 4;        // 序号
        uint8_t PCIType : 4;   // 帧类型 = 2
#elif ISOTP_BITFIELD_BIG_ENDIAN
        uint8_t PCIType : 4;   // 帧类型 = 2
        uint8_t SN : 4;        // 序号
#endif
        uint8_t Payload[7];     // 数据负载
    } byte;
    uint8_t data[8];
} Isotp_ConsecutiveFrame_t;


typedef struct
{
    Isotp_SingleFrame_t SF;      // 单帧
    Isotp_FirstFrame_t FF;       // 首帧
    Isotp_FlowControlFrame_t FC; // 流控帧
    Isotp_ConsecutiveFrame_t CF; // 连续帧
} Isotp_Obj;

typedef Isotp_Obj *Isotp_Handle_t;

#ifdef __cplusplus
}
#endif

#endif