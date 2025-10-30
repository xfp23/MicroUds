/**
 * @file Queue_types.h
 * @author https://github.com/xfp23
 * @brief Type definitions for memory pool based linked-list queue
 * @version 0.1
 * @date 2025-09-12
 */

#ifndef QUEUE_TYPES_H
#define QUEUE_TYPES_H

#include "stdint.h"
#include "stddef.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Queue operation status codes
 */
typedef enum
{
    QUEUE_OK = 0,   /**< Operation successful */
    QUEUE_ERR,      /**< General error / invalid input */
    QUEUE_FULL,     /**< Queue is full (no free node available) */
    QUEUE_EMPTY,    /**< Queue is empty (no active node) */
} Queue_Sta_t;

/**
 * @brief Queue node structure (used in both active and free lists)
 *
 * The actual data payload is stored in a flexible array that follows this
 * structure in the memory pool.
 */
typedef struct Queue_List_t
{
    struct Queue_List_t *next; /**< Pointer to next node in the list */
    uint16_t len;              /**< Length of valid data in this node (bytes) */
    /* uint8_t data[]; */      /**< Flexible array member (in memory pool) */
} Queue_List_t;

/**
 * @brief Queue configuration structure
 */
typedef struct
{
    size_t size;      /**< Number of nodes in the pool (queue capacity) */
    size_t item_size; /**< Maximum number of bytes per node (max message length) */
} Queue_conf_t;

/**
 * @brief Queue object structure
 *
 * Maintains both free-list (unused nodes) and active-list (FIFO queue).
 */
typedef struct
{
    Queue_List_t *free_list;   /**< Head of free nodes list */
    Queue_List_t *active_head; /**< Head of active queue (FIFO) */
    Queue_List_t *active_tail; /**< Tail of active queue */
    size_t pool_size;          /**< Total number of nodes in the pool */
    size_t item_size;          /**< Maximum payload size per node */
    uint8_t *pool;             /**< Contiguous memory pool (nodes arranged sequentially) */
    size_t count;              /**< Current number of active nodes */
} Queue_Obj;

/**
 * @brief Queue handle (opaque pointer to Queue object)
 */
typedef Queue_Obj *Queue_handle_t;

#ifdef __cplusplus
}
#endif

#endif /* QUEUE_TYPES_H */
