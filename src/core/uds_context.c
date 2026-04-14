/**
 * @file    uds_context.c
 * @author  https://github.com/xfp23
 * @version 0.1
 * @date    2026/04/02
 */
/* Includes ------------------------------------------------------------------*/
#include "uds_context.h"

/* Private variables ---------------------------------------------------------*/
static Uds_Context_Obj_t uds_context_obj = {0};
static Uds_Context_Obj_t *const ctx = &uds_context_obj;

/* Exported functions --------------------------------------------------------*/
void Uds_Context_Init(void)
{
    for (int i = 0; i < 256; i++)
    {
        // 初始化已使用链表为空
        ctx->serv_entry[i].head = NULL;
        ctx->serv_entry[i].serivce_entry = NULL;
        
        // 初始化空闲链表，连接所有数组元素
        for (int j = 0; j < MICROUDS_SERVICE_SUPPORT_SUBFUNC_NUMBER - 1; j++)
        {
            ctx->serv_entry[i].sub_func[j].next = &ctx->serv_entry[i].sub_func[j + 1];
            ctx->serv_entry[i].sub_func[j].IsUsed = false;
            ctx->serv_entry[i].sub_func[j].sub_id = 0x00;
            ctx->serv_entry[i].sub_func[j].func = NULL;
        }
        
        // 最后一个节点
        ctx->serv_entry[i].sub_func[MICROUDS_SERVICE_SUPPORT_SUBFUNC_NUMBER - 1].next = NULL;
        ctx->serv_entry[i].sub_func[MICROUDS_SERVICE_SUPPORT_SUBFUNC_NUMBER - 1].IsUsed = false;
        ctx->serv_entry[i].sub_func[MICROUDS_SERVICE_SUPPORT_SUBFUNC_NUMBER - 1].sub_id = 0x00;
        ctx->serv_entry[i].sub_func[MICROUDS_SERVICE_SUPPORT_SUBFUNC_NUMBER - 1].func = NULL;
        
        // 空闲链表头指向第一个元素
        ctx->serv_entry[i].free_head = &ctx->serv_entry[i].sub_func[0];
    }
}

MicroUds_Status_t Uds_Context_RegisterSubFunc(uint8_t sid, Uds_SubFunc_t sub_func)
{
    if (sid == 0 || sid > 255 || sub_func.sub_id == 0)
    {
        return MICROUDS_ERR;
    }
    
    Uds_Service_t *service = &ctx->serv_entry[sid];
    
    // 检查是否有空闲节点
    if (service->free_head == NULL)
    {
        return MICROUDS_FULL;
    }
    
    // 从空闲链表中取出一个节点
    Uds_SubFuncNode_t *new_node = service->free_head;
    service->free_head = service->free_head->next;
    
    // 填充节点数据
    new_node->IsUsed = true;
    new_node->sub_id = sub_func.sub_id;
    new_node->func = sub_func.func;
    
    // 插入到已使用链表头部（O(1)操作）
    new_node->next = service->head;
    service->head = new_node;
    
    return MICROUDS_OK;
}

MicroUds_Status_t Uds_Context_RemoveSubFunc(uint8_t sid, uint16_t sub_id)
{
    if (sid == 0 || sid > 255 || sub_id == 0)
    {
        return MICROUDS_ERR;
    }
    
    Uds_Service_t *service = &ctx->serv_entry[sid];
    Uds_SubFuncNode_t *prev = NULL;
    Uds_SubFuncNode_t *curr = service->head;
    
    // 在已使用链表中查找
    while (curr != NULL)
    {
        if (curr->sub_id == sub_id && curr->IsUsed == true)
        {
            // 从已使用链表中移除
            if (prev == NULL)
            {
                service->head = curr->next;  // 删除的是头节点
            }
            else
            {
                prev->next = curr->next;
            }
            
            // 清空节点数据
            curr->func = NULL;
            curr->IsUsed = false;
            curr->sub_id = 0;
            
            // 加入空闲链表头部
            curr->next = service->free_head;
            service->free_head = curr;
            
            return MICROUDS_OK;
        }
        prev = curr;
        curr = curr->next;
    }
    
    return MICROUDS_NOT_FOUND;
}

MicroUds_Status_t Uds_Context_FindSubFunc(uint8_t sid, uint16_t sub_id, Uds_SubFunc_t *out)
{
    if (sid == 0 || sid > 255 || sub_id == 0 || out == NULL)
    {
        return MICROUDS_ERR;
    }
    
    Uds_Service_t *service = &ctx->serv_entry[sid];
    Uds_SubFuncNode_t *node = service->head;
    
    // 只遍历已使用链表
    while (node != NULL)
    {
        if (node->sub_id == sub_id && node->IsUsed == true)
        {
            out->func = node->func;
            out->sub_id = sub_id;
            return MICROUDS_OK;
        }
        node = node->next;
    }
    
    return MICROUDS_NOT_FOUND;
}

MicroUds_Status_t Uds_Context_RegisterService(Uds_SerivceFunc_t service)
{
    if(service.sid > 255 || service.sid == 0 || service.func == NULL)
    {
        return MICROUDS_ERR;
    }
    // O(1)查找
    ctx->serv_entry[service.sid].IsAckFuncAddress = service.IsAckFuncAddress;
    ctx->serv_entry[service.sid].serivce_entry = service.func;
    ctx->serv_entry[service.sid].IsActive = true;

    return MICROUDS_OK;
}

MicroUds_Status_t Uds_Context_RemoveService(uint8_t sid)
{
    if(sid > 255 || sid == 0 )
    {
        return MICROUDS_ERR;
    }

    ctx->serv_entry[sid].IsAckFuncAddress = false;
    ctx->serv_entry[sid].serivce_entry = NULL;
    ctx->serv_entry[sid].IsActive = false;

    return MICROUDS_OK;
}

MicroUds_Status_t Uds_Context_FindService(uint8_t sid,Uds_SerivceFunc_t *out)
{
    if(sid > 255 || sid == 0 || out == NULL)
    {
        return MICROUDS_ERR;
    }

    if(ctx->serv_entry[sid].IsActive == false)
    {
        return MICROUDS_NOT_FOUND; // 没注册过，返回失败
    }

    out->IsAckFuncAddress = ctx->serv_entry[sid].IsAckFuncAddress;
    out->func = ctx->serv_entry[sid].serivce_entry;

    return MICROUDS_OK;
}

