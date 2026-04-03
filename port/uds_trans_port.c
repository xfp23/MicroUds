/**
 * @file    uds_trans_port.c
 * @author  https://github.com/xfp23
 * @version 0.1
 * @date    2026-04-02
 * @brief   Transport layer porting interface for MicroUDS
 *
 * This file provides user-implemented callbacks for transport layer interaction.
 * The user must implement these functions according to the underlying hardware
 * (e.g., CAN, LIN, Ethernet).
 */

/* Includes ------------------------------------------------------------------*/
#include "uds_trans_port.h"

/* ========================================================================== */
/*                        Transport Layer - TX Interface                      */
/* ========================================================================== */

/**
 * @brief Send UDS response to tester (Response Address)
 *
 * This function is called by the UDS stack when a response needs to be sent.
 * The user should implement the actual transmission logic here.
 *
 * Typical implementation:
 * - CAN: Send via CAN ID (e.g., 0x7E8)
 * - LIN/Ethernet: Send via corresponding driver
 *
 * @param[in] data Pointer to response buffer
 * @param[in] len  Length of response data
 * 
 * @return Success : 0 Fail : 1
 */
int MicroUds_Trans_ResponseAddress(uint8_t *data, size_t len)
{
    /* TODO: Implement transmission logic */

    return 0;
}


/* ========================================================================== */
/*                        Transport Layer - RX Interface                      */
/* ========================================================================== */

/**
 * @brief Receive UDS request from Physical Address
 *
 * This function should be called by the user when a physical request frame
 * is received (e.g., CAN ID 0x7E0).
 *
 * The user should forward the received data to the UDS stack.
 *
 * @param[in] data Pointer to received data buffer
 * @param[in] len  Length of received data
 */
void MicroUds_Trans_PhysicalAddress(uint8_t *data, size_t len)
{
    /* TODO: Call UDS stack entry function */
}


/**
 * @brief Receive UDS request from Functional Address
 *
 * This function should be called when a functional request is received
 * (e.g., CAN ID 0x7DF).
 *
 * Functional addressing is typically used for broadcast requests.
 *
 * @param[in] data Pointer to received data buffer
 * @param[in] len  Length of received data
 */
void MicroUds_Trans_FunctionAddress(uint8_t *data, size_t len)
{
    /* TODO: Call UDS stack entry function */
}