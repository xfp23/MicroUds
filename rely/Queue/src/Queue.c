#include "Queue.h"
#include <stdlib.h>
#include <string.h>

Queue_Sta_t Queue_Init(Queue_handle_t *handle, const Queue_conf_t *conf)
{
    if (!handle || !conf || conf->size == 0 || conf->item_size == 0)
        return QUEUE_ERR;

    Queue_Obj *q = (Queue_Obj *)calloc(1, sizeof(Queue_Obj));
    if (!q)
        return QUEUE_ERR;

    q->pool_size = conf->size;
    q->item_size = conf->item_size;
    q->count = 0;

    size_t node_size = sizeof(Queue_List_t) + q->item_size;

    q->pool = (uint8_t *)calloc(conf->size, node_size);
    if (!q->pool) {
        free(q);
        return QUEUE_ERR;
    }

    q->free_list = NULL;
    for (size_t i = 0; i < conf->size; i++) {
        Queue_List_t *node = (Queue_List_t *)(q->pool + i * node_size);
        node->next = q->free_list;
        node->len = 0;
        q->free_list = node;
    }

    q->active_head = NULL;
    q->active_tail = NULL;

    *handle = q;
    return QUEUE_OK;
}

Queue_Sta_t Queue_Push(Queue_handle_t handle, const void *data, uint16_t len)
{
    if (!handle || !data)
        return QUEUE_ERR;

    if (len == 0 || len > handle->item_size)
        return QUEUE_ERR; 

    if (!handle->free_list)
        return QUEUE_FULL;

    Queue_List_t *node = handle->free_list;
    handle->free_list = node->next;

    node->len = len;
    uint8_t *payload = (uint8_t *)node + sizeof(Queue_List_t);
    memcpy(payload, data, len);
    node->next = NULL;

    if (!handle->active_head) {
        handle->active_head = node;
        handle->active_tail = node;
    } else {
        handle->active_tail->next = node;
        handle->active_tail = node;
    }

    handle->count++;
    return QUEUE_OK;
}

Queue_Sta_t Queue_Pop(Queue_handle_t handle, void *out_buf, uint16_t *out_len)
{
    if (!handle || !out_buf || !out_len)
        return QUEUE_ERR;

    if (!handle->active_head)
        return QUEUE_EMPTY;

    Queue_List_t *node = handle->active_head;
    handle->active_head = node->next;
    if (!handle->active_head)
        handle->active_tail = NULL;

    uint8_t *payload = (uint8_t *)node + sizeof(Queue_List_t);
    uint16_t actual_len = node->len;

    memcpy(out_buf, payload, actual_len);
    *out_len = actual_len;

    node->next = handle->free_list;
    node->len = 0;
    handle->free_list = node;

    handle->count--;
    return QUEUE_OK;
}

size_t Queue_Count(Queue_handle_t handle)
{
    if (!handle) return 0;
    return handle->count;
}

size_t Queue_FreeCount(Queue_handle_t handle)
{
    if (!handle) return 0;

    return handle->pool_size - handle->count;
}

void Queue_Deinit(Queue_handle_t *handle)
{
    if (!handle || !*handle) return;

    Queue_Obj *q = *handle;
    if (q->pool) {
        free(q->pool);
        q->pool = NULL;
    }
    free(q);
    *handle = NULL;
}
