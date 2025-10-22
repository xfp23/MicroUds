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

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum
{
    ISOTP_OK = 0,
    ISOTP_ERR_PARAM,
    ISOTP_ERR_LENGTH,
    ISOTP_ERR_TYPE
} Isotp_Sta_t;

typedef enum
{
    FRAME_FIRST = 0x01,       // 首帧
    FRAME_CONSECUTIVE = 0x02, // 连续帧
    FRAME_FLOWCONTROL = 0x03, // 流控帧
} Isotp_FrameType_t;

/**
 * @brief ISO-TP Flow Control 帧定义
 */

typedef union
{
    struct
    {
        uint8_t PCI_DL : 4;        // 低 4 位：数据长度（Payload Length）
        uint8_t PCIType : 4; // 高 4 位：帧类型（SingleFrame = 0x0）
        uint8_t Payload[7];        // 数据区（最大 7 字节）
    } byte;

    uint8_t data[8];   // 原始帧数据
} Isotp_SingleFrame_t; // 单帧

typedef union
{
    struct
    {
        uint8_t FF_DL_H : 4;       // 高 4 位：消息总长度的高 4 位（12-bit length high nibble）
        uint8_t PCIType : 4; // 低 4 位：帧类型（First Frame = 0x1）
        uint8_t FF_DL_L;           // 低 8 位：消息总长度的低 8 位
        uint8_t Payload[6];        // 数据段（首帧最多携带 6 字节）
    } byte;

    uint8_t data[8];  // 原始 8 字节 CAN 帧
} Isotp_FirstFrame_t; // 首帧

typedef union
{
    struct
    {
        uint8_t FS : 4;            /**< FlowStatus: 0=CTS, 1=WAIT, 2=OVFLW */
        uint8_t PCIType : 4; /**< 帧类型: 固定为 0x3 表示 FlowControl */
        uint8_t BS;                /**< Block Size, 允许的连续帧数量 (0=无限制) */
        uint8_t STmin;             /**< 连续帧最小间隔时间 */
        uint8_t Reserve[5];        /**< 填充字节（置0） */
    } byte;

    uint8_t data[8];
} Isotp_FlowControlFrame_t; // 流控帧

typedef union
{
    struct
    {
        uint8_t SN : 4;            // 序列号 (Sequence Number, 0–15)
        uint8_t PCIType : 4; // 协议控制信息类型 (0x2 = Consecutive Frame)
        uint8_t Payload[7];        // 数据负载 (最多 7 字节)
    } byte;

    uint8_t data[8];        // 原始 8 字节 CAN 帧
} Isotp_ConsecutiveFrame_t; // 连续帧

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