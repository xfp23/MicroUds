/**
 * @file Queue.h
 * @author https://github.com/xfp23
 * @brief Pooled queue implementation (free list + active list), supports variable-length messages (enqueue with len)
 * @date 2025-09-18
 */

#ifndef QUEUE_H
#define QUEUE_H

#include "Queue_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Initialize a queue object
 *
 * @param handle Pointer to queue handle
 * @param conf   Pointer to queue configuration (pool size, max item size)
 * @return Queue_Sta_t QUEUE_OK on success, QUEUE_ERR on failure
 */
extern Queue_Sta_t Queue_Init(Queue_handle_t *handle, const Queue_conf_t *conf);

/**
 * @brief Push a new item into the queue
 *
 * @param handle Queue handle
 * @param data   Pointer to the data to be enqueued
 * @param len    Data length in bytes (must not exceed configured max size)
 * @return Queue_Sta_t QUEUE_OK on success, QUEUE_FULL if no free node, QUEUE_ERR on invalid input
 */
extern Queue_Sta_t Queue_Push(Queue_handle_t handle, const void *data, uint16_t len);

/**
 * @brief Pop an item from the queue
 *
 * @param handle   Queue handle
 * @param out_buf  Buffer to receive the data
 * @param out_len  Output length of the received data
 * @return Queue_Sta_t QUEUE_OK on success, QUEUE_EMPTY if queue is empty, QUEUE_ERR on invalid input
 */
extern Queue_Sta_t Queue_Pop(Queue_handle_t handle, void *out_buf, uint16_t *out_len);

/**
 * @brief Get the number of items currently stored in the queue
 *
 * @param handle Queue handle
 * @return size_t Number of active nodes
 */
size_t Queue_Count(Queue_handle_t handle);

/**
 * @brief Get the number of available free nodes
 *
 * @param handle Queue handle
 * @return size_t Number of free nodes
 */
size_t Queue_FreeCount(Queue_handle_t handle);

/**
 * @brief Destroy a queue object and free all allocated memory
 *
 * @param handle Pointer to queue handle, will be set to NULL after deinit
 */
void Queue_Deinit(Queue_handle_t *handle);

#ifdef __cplusplus
}
#endif

#endif /* QUEUE_H */
