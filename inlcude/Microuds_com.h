#ifndef MICROUDS_COM_H
#define MICROUDS_COM_H

/**
 * @file    MicroUDS_com.h
 * @author  https://github.com/xfp23
 * @brief   Common utility macros for MicroUDS core
 * @version 0.1
 * @date    2025-10-21
 *
 * @copyright
 * Copyright (c) 2025
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Checks whether a pointer is NULL.
 * 
 * If @p ptr is NULL, the function immediately returns
 * `MICROUDS_ERR_PARAM`.
 */
#define MICROUDS_CHECKPTR(ptr)         \
    do                                 \
    {                                  \
        if ((ptr) == NULL)             \
            return MICROUDS_ERR_PARAM; \
    } while (0)

/**
 * @brief Safely calls the user-defined transmit function.
 * 
 * Ensures that @ref MicroUDS_Handle->Transmit is valid before use.
 * If the transmit function pointer is NULL or returns a non-zero
 * error code, this macro returns `MICROUDS_ERR_TRANS`.
 *
 * @param buf  Pointer to the data buffer to be sent.
 * @param len  Number of bytes to send.
 */
#define MICROUDS_SAFE_CALL_TRANSMIT(buf, len)             \
    do                                                    \
    {                                                     \
        if (MicroUDS_Handle->Transmit == NULL)            \
            return MICROUDS_ERR_TRANS;                    \
        if (MicroUDS_Handle->Transmit((buf), (len)) != 0) \
            return MICROUDS_ERR_TRANS;                    \
    } while (0)

/**
 * @brief Offset value added to a UDS SID for positive response.
 * 
 * Example: If request SID = 0x10, the response SID = 0x50 (0x10 + 0x40).
 */
#define MICROUDS_RESPONSE_OFFSET 0x40

/**
 * @brief Checks whether the ECU is busy before executing a new request.
 * 
 * If ECU status is @c ECU_BUSY, a "Busy Repeat Request" negative response
 * (NRC 0x21) is sent immediately, and the function returns.
 */
#define UDS_CHECK_ECU_BUSY()                                        \
    do                                                              \
    {                                                               \
        if (MicroUDS_Handle->Ecu_sta == ECU_BUSY)                   \
        {                                                           \
            MicroUDS_NegativeResponse(UDS_NRC_BUSY_REPEAT_REQUEST); \
            return;                                                 \
        }                                                           \
    } while (0)

/**
 * @brief Marks the ECU status as busy.
 */
#define MICROUDS_ECUSETBUSY()   (MicroUDS_Handle->Ecu_sta = ECU_BUSY)

/**
 * @brief Clears the ECU busy flag, marking it as free.
 */
#define MICROUDS_ECUCLEAR()     (MicroUDS_Handle->Ecu_sta = ECU_FREE)

/**
 * @brief Converts milliseconds to system ticks.
 * 
 * Uses the configured @ref MICROUDS_TICK_FREQ_HZ for conversion.
 */
#define MICROUDS_MS_TICK(ms)    ((ms) * (MICROUDS_TICK_FREQ_HZ / 1000))

/**
 * @brief Returns the number of elements in an array.
 * 
 * Example:
 * @code
 * MicroUDS_ServiceTable_t table[] = { ... };
 * size_t n = MICROUDS_COUNTOF(table);
 * @endcode
 */
#define MICROUDS_COUNTOF(arr)   (sizeof(arr) / sizeof((arr)[0]))


#ifdef __cplusplus
}
#endif

#endif /* MICROUDS_COM_H */
