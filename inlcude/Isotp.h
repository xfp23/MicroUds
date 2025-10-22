#ifndef ISOTP_H
#define ISOTP_H

/**
 * @file Isotp.h
 * @author 
 *      https://github.com/xfp23
 * @brief 
 *      ISO-TP (ISO 15765-2) protocol layer API definitions.
 *      Provides frame packing/unpacking interfaces for Single Frame (SF),
 *      First Frame (FF), Consecutive Frame (CF), and Flow Control (FC) frames.
 * @version 0.1
 * @date 2025-10-22
 * 
 * @copyright 
 *      Copyright (c) 2025
 */

#include "Isotp_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Global ISO-TP protocol handle.
 */
extern Isotp_Handle_t Isotp_Handle;

/**
 * @brief Pack an ISO-TP Single Frame (SF) into a CAN frame buffer.
 *
 * This function constructs a Single Frame according to ISO 15765-2 format.
 * The Single Frame is used for messages up to 7 bytes.
 *
 * @param Dst  Pointer to destination CAN frame buffer (8 bytes).
 * @param Src  Pointer to source data payload (≤ 7 bytes).
 * @param size Length of payload in bytes (1–7).
 * @return Isotp_Sta_t 
 *         - ISOTP_OK:        Frame packed successfully.  
 *         - ISOTP_ERR_PARAM: Invalid input pointers.  
 *         - ISOTP_ERR_LENGTH: Invalid payload length.
 */
extern Isotp_Sta_t Isotp_PackSingleFrame(uint8_t *Dst, const uint8_t *Src, size_t size);

/**
 * @brief Unpack a Single Frame (SF) from a raw CAN frame.
 *
 * The received 8-byte CAN data is parsed into an ISO-TP Single Frame structure.
 *
 * @param Dst Pointer to ISO-TP Single Frame structure to store parsed data.
 * @param Src Pointer to raw CAN frame data (8 bytes).
 * @return Isotp_Sta_t 
 *         - ISOTP_OK: Frame unpacked successfully.  
 *         - ISOTP_ERR_PARAM: Invalid pointers.
 */
extern Isotp_Sta_t Isotp_UnpackSingleFrame(Isotp_SingleFrame_t *Dst, const uint8_t *Src);

/**
 * @brief Pack a First Frame (FF) for multi-frame ISO-TP transmission.
 *
 * The First Frame is used when total payload length exceeds 7 bytes.
 * It contains the total message length and the first 6 bytes of data.
 *
 * @param Dst  Pointer to destination CAN frame buffer (8 bytes).
 * @param Src  Pointer to source data payload (total message).
 * @param size Total message length (8–4095 bytes, 12-bit value).
 * @return Isotp_Sta_t 
 *         - ISOTP_OK:        Frame packed successfully.  
 *         - ISOTP_ERR_PARAM: Invalid pointers.  
 *         - ISOTP_ERR_LENGTH: Invalid message length.
 */
extern Isotp_Sta_t Isotp_PackFirstFrame(uint8_t *Dst, const uint8_t *Src, size_t size);

/**
 * @brief Unpack a First Frame (FF) from raw CAN data.
 *
 * Extracts total length and first data segment from the received First Frame.
 *
 * @param Dst Pointer to First Frame structure to store parsed data.
 * @param Src Pointer to raw CAN frame data (8 bytes).
 * @return Isotp_Sta_t 
 *         - ISOTP_OK: Frame unpacked successfully.  
 *         - ISOTP_ERR_PARAM: Invalid pointers.
 */
extern Isotp_Sta_t Isotp_UnpackFirstFrame(Isotp_FirstFrame_t *Dst, const uint8_t *Src);

/**
 * @brief Pack a Flow Control (FC) frame to control multi-frame transmission.
 *
 * The Flow Control frame is used by the receiver to instruct the sender
 * whether to continue, wait, or stop sending consecutive frames.
 *
 * @param Dst    Pointer to destination CAN frame buffer (8 bytes).
 * @param bs     Block Size (number of consecutive frames allowed, 0 = unlimited).
 * @param STime  Minimum separation time (STmin) between frames.
 * @param fs     Flow status (FS_CTS, FS_WAIT, or FS_OVFLW).
 * @return Isotp_Sta_t 
 *         - ISOTP_OK: Frame packed successfully.  
 *         - ISOTP_ERR_PARAM: Invalid pointer.
 */
extern Isotp_Sta_t Isotp_PackFlowControlFrame(uint8_t *Dst, uint8_t bs, uint8_t STime, Isotp_FlowStatus_t fs);

/**
 * @brief Unpack a Flow Control (FC) frame from raw CAN data.
 *
 * Parses Flow Status (FS), Block Size (BS), and Separation Time (STmin) fields.
 *
 * @param Dst Pointer to Flow Control Frame structure to store parsed data.
 * @param Src Pointer to raw CAN frame data (8 bytes).
 * @return Isotp_Sta_t 
 *         - ISOTP_OK: Frame unpacked successfully.  
 *         - ISOTP_ERR_PARAM: Invalid pointers.
 */
extern Isotp_Sta_t Isotp_UnpackFlowControlFrame(Isotp_FlowControlFrame_t *Dst, uint8_t *Src);

/**
 * @brief Pack a Consecutive Frame (CF) for segmented ISO-TP data transmission.
 *
 * Consecutive Frames are used to send the remaining data after the First Frame.
 *
 * @param Dst  Pointer to destination CAN frame buffer (8 bytes).
 * @param Src  Pointer to data payload for this CF (≤ 7 bytes).
 * @param size Payload length (1–7 bytes).
 * @param SN   Sequence number (0–15).
 * @return Isotp_Sta_t 
 *         - ISOTP_OK: Frame packed successfully.  
 *         - ISOTP_ERR_PARAM: Invalid pointers.  
 *         - ISOTP_ERR_LENGTH: Invalid payload length.
 */
extern Isotp_Sta_t Isotp_PackConsecutiveFrame(uint8_t *Dst, uint8_t *Src, size_t size, uint8_t SN);

/**
 * @brief Unpack a Consecutive Frame (CF) from raw CAN data.
 *
 * Extracts sequence number (SN) and payload content.
 *
 * @param Dst Pointer to Consecutive Frame structure to store parsed data.
 * @param Src Pointer to raw CAN frame data (8 bytes).
 * @return Isotp_Sta_t 
 *         - ISOTP_OK: Frame unpacked successfully.  
 *         - ISOTP_ERR_PARAM: Invalid pointers.  
 *         - ISOTP_ERR_TYPE: Frame type mismatch.  
 *         - ISOTP_ERR_FRAME: Invalid frame sequence or format.
 */
extern Isotp_Sta_t Isotp_UnPackConsecutiveFrame(Isotp_ConsecutiveFrame_t *Dst, uint8_t *Src);

#ifdef __cplusplus
}
#endif

#endif /* ISOTP_H */
