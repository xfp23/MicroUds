/**
 * @file MicroHash.c
 * @author
 * @brief
 * @version 0.1
 * @date 2025-10-28
 *
 * @copyright Copyright (c) 2025
 *
 */

#include "MicroHash.h"
#include "MicroHash_com.h"
#include "stdlib.h"

/**
 * @brief 根据id返回桶内的索引数据
 *
 * @param key
 * @return uint32_t
 */
static inline uint32_t getIndex(uint32_t key, size_t buckSize)
{
    return (uint32_t)((key * 1101524993u) % buckSize);
}

MicroHash_Sta_t MicroHash_Init(MicroHash_Handle_t *handle, MicroHash_Conf_t *conf)
{
    MICROHASH_CHECKPTR(handle);
    MICROHASH_CHECKPTR(conf);
    if (*handle != NULL)
        return MICROHASH_ERR_PARAM;

    *handle = (MicroHash_Obj *)calloc(1, sizeof(MicroHash_Obj)); // 分配hash表空间

    if (*handle == NULL)
        return MICROHASH_ERR_MEMORY;

    (*handle)->conf = *conf;
    (*handle)->Remain = conf->buckSize;

    (*handle)->buckets = (MicroHash_Node_t **)calloc((*handle)->conf.buckSize, sizeof(MicroHash_Node_t *));

    if ((*handle)->buckets == NULL)
    {
        (*handle)->buckets = NULL;
        free(*handle);
        return MICROHASH_ERR_MEMORY;
    }

    return MICROHASH_OK;
}

MicroHash_Sta_t MicroHash_Insert(MicroHash_Handle_t *handle, MicroHash_key_t key, void *data)
{
    MICROHASH_CHECKPTR(handle);
    MICROHASH_CHECKPTR(*handle);

    MicroHash_key_t index = getIndex(key, (*handle)->conf.buckSize);
    MicroHash_Node_t *head = (*handle)->buckets[index];
    MicroHash_Node_t *node = head;

    // 更新数据
    while (node != NULL)
    {
        if (node->key == key)
        {
            node->data = data;
            return MICROHASH_OK;
        }
        node = node->next;
    }

    MicroHash_Node_t *newNode = (MicroHash_Node_t *)calloc(1, sizeof(MicroHash_Node_t));
    if (newNode == NULL)
        return MICROHASH_ERR_MEMORY;

    newNode->key = key;
    newNode->data = data;
    newNode->next = head;
    (*handle)->buckets[index] = newNode;

    (*handle)->Remain--;
    return MICROHASH_OK;
}

void *MicroHash_Find(MicroHash_Handle_t *handle, MicroHash_key_t key)
{
    MICROHASH_CHECKPTR(handle);
    MICROHASH_CHECKPTR(*handle);

    MicroHash_key_t index = getIndex(key, (*handle)->conf.buckSize);
    MicroHash_Node_t *node = (*handle)->buckets[index];

    while (node)
    {
        if (node->key == key)
            return node->data;
        node = node->next;
    }
    return NULL;
}

MicroHash_Sta_t MicroHash_Remove(MicroHash_Handle_t *handle, MicroHash_key_t key)
{
    MICROHASH_CHECKPTR(handle);
    MICROHASH_CHECKPTR(*handle);

    MicroHash_key_t index = getIndex(key, (*handle)->conf.buckSize);
    MicroHash_Node_t *node = (*handle)->buckets[index];
    MicroHash_Node_t *prev = NULL;

    while (node)
    {
        if (node->key == key)
        {
            if (prev)
                prev->next = node->next;
            else
                (*handle)->buckets[index] = node->next;

            free(node);
            (*handle)->Remain++;
            return MICROHASH_OK;
        }
        prev = node;
        node = node->next;
    }
    return MICROHASH_ERR;
}

MicroHash_Sta_t MicroHash_Delete(MicroHash_Handle_t *handle)
{
    if (handle == NULL || *handle == NULL)
        return MICROHASH_ERR_PARAM;

    for (size_t i = 0; i < (*handle)->conf.buckSize; i++)
    {
        MicroHash_Node_t *node = (*handle)->buckets[i];
        while (node)
        {
            MicroHash_Node_t *next = node->next;
            free(node);
            node = next;
        }
        (*handle)->buckets[i] = NULL;
    }

    free((*handle)->buckets);
    (*handle)->buckets = NULL;

    free(*handle);
    *handle = NULL;

    return MICROHASH_OK;
}
