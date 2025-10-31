#ifndef MICROUDS_H
#define MICROUDS_H

/**
 * @file Microuds.h
 * @author
 *    https://github.com/xfp23
 * @brief
 *    MicroUDS - Lightweight UDS (ISO 14229) diagnostic protocol stack.
 *    This header defines the public API for initialization, service registration,
 *    timer handling, and data processing.
 * @version 0.1
 * @date 2025-10-21
 *
 * @copyright
 *    Copyright (c) 2025
 *
 */

#include "Microuds_types.h"
#include "Isotp.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Global UDS handle instance.
 *
 * This object contains all internal runtime states and buffers.
 * It should be initialized by calling @ref MicroUDS_Init().
 */
extern MicroUDS_Handle_t const MicroUDS_Handle;

/**
 * @brief Initialize the MicroUDS module.
 *
 * @param conf Pointer to configuration structure @ref MicroUDS_Conf_t.
 * @return MicroUDS_Sta_t Initialization result.
 * - MICROUDS_OK: Initialization successful.
 * - MICROUDS_ERR_PARAM: Invalid configuration.
 * - MICROUDS_ERR_MEMORY: Memory allocation failure.
 */
extern MicroUDS_Sta_t MicroUDS_Init(MicroUDS_Conf_t *conf);

/**
 * @brief UDS tick handler, should be called periodically (e.g., every 1 ms).
 *
 * Used for managing timeout counters and protocol timers.
 */
void MicroUDS_TickHandler(void);

/**
 * @brief Reset internal timer counters.
 *
 * This function is typically called when a valid frame or flow control is received.
 */
void MicroUDS_ResetTimer(void);

/**
 * @brief Handle timeout-related events and pending requests.
 *
 * Call this function in a main loop
 * It will automatically generate NRC (negative response) on timeout.
 */
void MicroUDS_TimerHandler(void);

/**
 * @brief Receive callback for incoming ISO-TP frame data.
 *
 * This function should be called by the transport layer (ISO-TP)
 * each time a complete CAN frame is received.
 *
 * @param data Pointer to received 8-byte CAN frame data.
 */
void MicroUDS_ReceiveCallback(uint8_t *data);

/**
 * @brief Register a table of UDS services (SID-level handlers).
 *
 * @param table Pointer to an array of service descriptors.
 * @param table_len Length of the service table.
 * @return MicroUDS_Sta_t Registration result.
 * - MICROUDS_OK: Registration successful.
 * - MICROUDS_ERR_PARAM: Invalid pointer or length.
 * - MICROUDS_ERR_HASH: Failed to insert into hash table.
 */
MicroUDS_Sta_t MicroUDS_RegisterService(MicroUDS_ServiceTable_t *table, size_t table_len);

/**
 * @brief Register session handlers (SSID-level) under a specific Service ID.
 *
 * @param sid Service ID to register under.
 * @param table Pointer to a session (sub-function) table.
 * @param table_len Length of the table.
 * @return MicroUDS_Sta_t Registration result.
 */
MicroUDS_Sta_t MicroUDS_RegisterSession(MicroUDS_Sid_t sid, MicroUDS_SessionTable_t *table, size_t table_len);

/**
 * @brief Send a standard positive response (0x50-type).
 *
 * Typically called after a service handler completes successfully.
 *
 * @return MicroUDS_Sta_t Transmission result.
 */
MicroUDS_Sta_t MicroUDS_PositiveResponse(void);

/**
 * @brief Send a negative response (0x7F-type).
 *
 * @param code NRC (Negative Response Code) defined in ISO 14229-1.
 * @return MicroUDS_Sta_t Transmission result.
 */
MicroUDS_Sta_t MicroUDS_NegativeResponse(MicroUDS_NRC_t code);

/**
 * @brief Delete or deinitialize the UDS instance.
 *
 * Frees any allocated memory and resets all state machines.
 */
void MicroUDS_Delete(void);

/**
 * @brief Retrieve pointer to reassembled multi-frame data.
 *
 * After a segmented transfer (First Frame + Consecutive Frames) completes,
 * this function can be used to obtain the full payload.
 *
 * @param[out] out_buffer Pointer to store buffer address.
 * @param[out] out_size Pointer to store total payload size.
 * @return MicroUDS_Sta_t Operation result.
 * - MICROUDS_OK: Data available.
 * - MICROUDS_ERR: No complete multi-frame received.
 */
MicroUDS_Sta_t MicroUDS_Multiframedata(uint8_t **out_buffer, size_t *out_size);

#ifdef __cplusplus
}
#endif

#endif /* MICROUDS_H */