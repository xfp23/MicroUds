/**
 * @file    MicroUDS_conf.h
 * @author  https://github.com/xfp23
 * @brief   MicroUDS Configuration File
 * @version 0.0.1
 * @date    2025-10-21
 *
 * @copyright
 * Copyright (c) 2025
 */

#ifndef MICROUDS_CONF_H
#define MICROUDS_CONF_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*                              Version & General                             */
/* -------------------------------------------------------------------------- */

/**
 * @brief MicroUDS version string (for user reference only).
 */
#define MICROUDS_VERSION_STR          "0.0.1"

/**
 * @brief Hash table size — corresponds to the number of supported UDS services.
 *
 * Each registered service occupies one hash bucket.
 * 
 * ⚙️ Recommended: set equal or slightly higher than the number of services
 * you plan to register.
 */
#define MICROUDS_HASH_SIZE            32

/**
 * @brief System tick frequency in Hz.
 * 
 * Defines the timing base used by all internal timers.
 * This should match your periodic scheduler or timer callback rate.
 * 
 * Example: 1ms tick → 1000Hz.
 */
#define MICROUDS_TICK_FREQ_HZ         1000


/* -------------------------------------------------------------------------- */
/*                              Timing Parameters                             */
/* -------------------------------------------------------------------------- */

/**
 * @brief Inter-frame timeout for multi-frame transmission (N_Cs).
 * 
 * If this timeout expires during transmission or reception,
 * the ongoing transfer will be aborted.
 *
 * Unit: milliseconds.
 */
#define MICROUDS_TIMEOUT_N_CS_MS      150

/**
 * @brief Maximum service execution timeout.
 * 
 * If the service execution exceeds this time,
 * the active session will be reset to the default session.
 *
 * Unit: milliseconds.
 */
#define MICROUDS_SERVICE_TIMEOUT_MS   5000


/* -------------------------------------------------------------------------- */
/*                           User Callback Functions                          */
/* -------------------------------------------------------------------------- */

/**
 * @brief User-defined transmit callback (see @ref MicroUDS_TransmitFunc_t).
 *
 * The callback should send a CAN (or similar transport) frame.
 * 
 * Example:
 * @code
 * int MyCAN_Transmit(uint8_t *data, size_t len);
 * #define MICROUDS_TRANSMIT_CB MyCAN_Transmit
 * @endcode
 */
#define MICROUDS_TRANSMIT_CB          NULL


/* -------------------------------------------------------------------------- */
/*                             Service Record Config                          */
/* -------------------------------------------------------------------------- */

/**
 * @brief Number of UDS service records available.
 * 
 * This defines how many service entries can be registered in MicroUDS.
 * It also controls memory allocation used by @ref MicroUDS_RegisterService.
 *
 * ⚠️ If this value is too small, service registration may overwrite existing
 * entries or trigger memory corruption.
 */
#define MICROUDS_SERVICE_RECORDS      2


/* -------------------------------------------------------------------------- */
/*                              Sanity Checks                                 */
/* -------------------------------------------------------------------------- */

#ifndef MICROUDS_TRANSMIT_CB
#warning "MICROUDS_TRANSMIT_CB not defined — defaulting to NULL (no transmission)."
#define MICROUDS_TRANSMIT_CB NULL
#else
/**
 * @brief Extern declaration of user-defined transmit function.
 * 
 * This function must be provided by the application layer.
 * 
 * @param data Pointer to data buffer to transmit.
 * @param len  Number of bytes to transmit.
 * @return 0 on success, non-zero on failure.
 */
extern int MICROUDS_TRANSMIT_CB(uint8_t *data, size_t len);
#endif


#ifdef __cplusplus
}
#endif

#endif /* MICROUDS_CONF_H */
