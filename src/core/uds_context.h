/**
 * @file    uds_context.h
 * @author  https://github.com/xfp23
 * @version 0.1
 * @date    2026/04/02
 */

#ifndef UDS_CONTEXT_H_
#define UDS_CONTEXT_H_

/* Includes ------------------------------------------------------------------*/
#include "MicroUds_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct
{
    uint16_t sub_id;
    void (*func)(void);
} Uds_SubFunc_t;

typedef struct {
    uint8_t sid;
    bool IsAckFuncAddress;
    void (*func)(uint8_t *data,size_t len);
} Uds_SerivceFunc_t;

typedef struct Uds_SubFunction_t
{
    bool IsUsed;
    uint16_t sub_id;
    void (*func)(void);
    struct Uds_SubFunction_t *next;  // 指向下一个已使用的节点
} Uds_SubFuncNode_t;

typedef struct
{
    bool IsActive; // 此服务是否被激活
    bool IsAckFuncAddress;
    void (*serivce_entry)(uint8_t *data,size_t len);
    Uds_SubFuncNode_t sub_func[MICROUDS_SERVICE_SUPPORT_SUBFUNC_NUMBER];
    Uds_SubFuncNode_t *head;  // 指向第一个已使用的节点
    Uds_SubFuncNode_t *free_head;  // 指向第一个空闲节点
} Uds_Service_t;

/* Exported types ------------------------------------------------------------*/
typedef struct
{
    Uds_Service_t serv_entry[256];
} Uds_Context_Obj_t;

/* Exported constants --------------------------------------------------------*/
extern void Uds_Context_Init(void);
extern MicroUds_Status_t Uds_Context_RegisterSubFunc(uint8_t sid, Uds_SubFunc_t sub_func);
extern MicroUds_Status_t Uds_Context_RemoveSubFunc(uint8_t sid, uint16_t sub_id);
extern MicroUds_Status_t Uds_Context_FindSubFunc(uint8_t sid, uint16_t sub_id, Uds_SubFunc_t *out);
extern MicroUds_Status_t Uds_Context_RegisterService(Uds_SerivceFunc_t service);
extern MicroUds_Status_t Uds_Context_RemoveService(uint8_t sid);
extern MicroUds_Status_t Uds_Context_FindService(uint8_t sid,Uds_SerivceFunc_t *out);

#ifdef __cplusplus
}
#endif

#endif /* UDS_CONTEXT_H_ */ 