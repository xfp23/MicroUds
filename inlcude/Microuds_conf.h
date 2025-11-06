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

#include "stdint.h"
#include "stddef.h"

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
 * Each registered service occupies one hash bucket.
 */
#define MICROUDS_HASH_SIZE            32

/**
 * @brief System tick frequency in Hz.
 * 
 * Defines the timing base used by all MicroUDS internal timers.
 * This should match the period of your scheduler or periodic timer callback.
 */
#define MICROUDS_TICK_FREQ_HZ         1000


/* -------------------------------------------------------------------------- */
/*                              Timing Parameters                             */
/* -------------------------------------------------------------------------- */

/**
 * @brief Inter-frame timeout for multi-frame transmission (N_Cs).
 * 
 * If exceeded, the ongoing transfer will be aborted.
 * Unit: milliseconds.
 */
#define MICROUDS_TIMEOUT_N_CS_MS      150

/**
 * @brief Maximum service execution timeout.
 * 
 * If this timeout is reached, the session will be reset to default.
 * Unit: milliseconds.
 */
#define MICROUDS_SERVICE_TIMEOUT_MS   5000


/* -------------------------------------------------------------------------- */
/*                           User Callback Functions                          */
/* -------------------------------------------------------------------------- */

/**
 * @brief User-defined transmit callback. @ref MicroUDS_TransmitFunc_t
 *
 * @param data Pointer to the data buffer to send.
 * @param len  Number of bytes to send.
 * @return Status code defined by user (0 = OK, non-zero = error).
 *
 * @note Assign this macro to your transmit function, e.g.:
 *       @code
 *       #define MICROUDS_TRANSMIT_CB   MyCAN_Transmit
 *       @endcode
 */
#define MICROUDS_TRANSMIT_CB          NULL


/* -------------------------------------------------------------------------- */
/*                             Service Record Config                           */
/* -------------------------------------------------------------------------- */

/**
 * @brief Number of UDS service records available.
 * 
 * This value determines how many service entries can be registered
 * within MicroUDS. It also serves the deletion logic in
 * @ref MicroUDS_Delete.
 *
 * ⚠️ If too small, new service registrations may overwrite existing ones
 * or cause memory corruption.
 */
#define MICROUDS_SERVICE_RECORDS      2

/* -------------------------------------------------------------------------- */
/*                              Sanity Checks                                 */
/* -------------------------------------------------------------------------- */

#ifndef MICROUDS_TRANSMIT_CB 
#warning "MICROUDS_TRANSMIT_CB not defined. Defaulting to NULL (no transmission)."
#define MICROUDS_TRANSMIT_CB NULL
#else
extern int MICROUDS_TRANSMIT_CB(uint8_t *data,size_t len);
#endif

#ifdef __cplusplus
}
#endif

#endif /* MICROUDS_CONF_H */
