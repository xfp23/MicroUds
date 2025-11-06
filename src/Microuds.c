/**
 * @file Microuds.c
 * @author https://github.com/xfp23
 * @brief uds协议栈
 * @version 0.1
 * @date 2025-10-31
 *
 * @copyright Copyright (c) 2025
 *
 */

#include "Microuds.h"
#include "Microuds_com.h"
#include "stdlib.h"
#include "string.h"

static MicroUDS_Obj MicroUDS = {0};
MicroUDS_Handle_t const MicroUDS_Handle = &MicroUDS;

static void MicroUDS_ClearRecv(void);

/**
 * @brief 给一个响应
 *
 * @param code NRC码
 */
static inline void MicroUDS_Response(MicroUDS_NRC_t code);

MicroUDS_Sta_t MicroUDS_PositiveResponse(void)
{
    uint8_t data[8] = {0};
    uint8_t res[8] = {0};
    size_t len = 0;

    /* SID + 0x40 表示正响应 */
    data[0] = (uint8_t)(MicroUDS_Handle->sid + UDS_RESPONSE_OFFSET);
    len = 1;

    if (MicroUDS_Handle->ssid != 0)
        data[len++] = (uint8_t)MicroUDS_Handle->ssid;

    if (Isotp_PackSingleFrame(res, data, (uint8_t)len) != ISOTP_OK)
        return MICROUDS_ERR;

    /* 始终发送完整 8 字节 CAN 帧 */
    MICROUDS_SAFE_CALL_TRANSMIT(res, 8);

    return MICROUDS_OK;
}

MicroUDS_Sta_t MicroUDS_NegativeResponse(MicroUDS_NRC_t code)
{
    uint8_t data[8] = {0};
    uint8_t res[8] = {0};

    data[0] = 0x7F;
    data[1] = (uint8_t)MicroUDS_Handle->sid;
    data[2] = (uint8_t)code;

    if (Isotp_PackSingleFrame(res, data, 3) != ISOTP_OK)
        return MICROUDS_ERR;

    /* 始终发送完整 8 字节 CAN 帧 */
    MICROUDS_SAFE_CALL_TRANSMIT(res, 8);

    return MICROUDS_OK;
}

MicroUDS_Sta_t MicroUDS_Init(void)
{

    if (MICROUDS_HASH_SIZE == 0)
        return MICROUDS_ERR_PARAM;

    memset(&MicroUDS, 0, sizeof(MicroUDS));

    MicroHash_Conf_t hashConf = {
        .buckSize = MICROUDS_HASH_SIZE,
    };

    /* 注册回调 */
    MicroUDS_Handle->Transmit = MICROUDS_TRANSMIT_CB;

    /* 分配记录表与初始化计数 */
    MicroUDS_Handle->Record.data = (uint8_t *)calloc(MICROUDS_SERVICE_RECORDS, sizeof(uint8_t));
    if (MicroUDS_Handle->Record.data == NULL)
        return MICROUDS_ERR_MEMORY;
    MicroUDS_Handle->Record.count = 0;
    MicroUDS_Handle->Record.size = MICROUDS_SERVICE_RECORDS;

    /* 初始化哈希表 */
    MicroHash_Sta_t HashRet = MicroHash_Init(&MicroUDS_Handle->hashTable, &hashConf);
    if (HashRet != MICROHASH_OK)
    {
        free(MicroUDS_Handle->Record.data);
        MicroUDS_Handle->Record.data = NULL;
        MicroUDS_Handle->Record.size = 0;
        MicroUDS_Handle->Record.count = 0;
        switch (HashRet)
        {
        case MICROHASH_ERR:
            return MICROUDS_ERR;
        case MICROHASH_ERR_PARAM:
            return MICROUDS_ERR_HASH;
        case MICROHASH_ERR_MEMORY:
            return MICROUDS_ERR_MEMORY;
        default:
            return MICROUDS_ERR;
        }
    }

    /* 初始化会话为默认会话 */
    MicroUDS_Handle->sid = UDS_DIAGNOSTIC_SESSION_CONTROL;
    MicroUDS_Handle->ssid = UDS_SESSION_DEFAULT;
    MicroUDS_Handle->last_time = 0;
    MicroUDS_Handle->Tick = 0;
    MicroUDS_Handle->Timeout = MICROUDS_SERVICE_TIMEOUT_MS;

    return MICROUDS_OK;
}

void MicroUDS_Delete(void)
{
    if (MicroUDS_Handle->Record.data)
    {
        for (size_t i = 0; i < MicroUDS_Handle->Record.count; i++)
        {
            MicroUDS_Sid_t sid = (MicroUDS_Sid_t)MicroUDS_Handle->Record.data[i];
            Microuds_Service_t *svc = (Microuds_Service_t *)MicroHash_Find(&MicroUDS_Handle->hashTable, (MicroHash_key_t)sid);
            if (!svc)
                continue;

            MicroUDS_Session_t *ses = svc->Session;
            while (ses)
            {
                MicroUDS_Session_t *next = ses->next;
                free(ses);
                ses = next;
            }
            free(svc);
        }

        free(MicroUDS_Handle->Record.data);
        MicroUDS_Handle->Record.data = NULL;
        MicroUDS_Handle->Record.count = 0;
        MicroUDS_Handle->Record.size = 0;
    }

    MicroHash_Delete(&MicroUDS_Handle->hashTable);

    memset(&MicroUDS, 0, sizeof(MicroUDS));
}

MicroUDS_Sta_t MicroUDS_RegisterService(MicroUDS_ServiceTable_t *table, size_t table_len)
{
    MICROUDS_CHECKPTR(table);
    if (table_len == 0)
        return MICROUDS_ERR_PARAM;

    for (size_t i = 0; i < table_len; i++)
    {
        Microuds_Service_t *svc = (Microuds_Service_t *)calloc(1, sizeof(Microuds_Service_t));
        if (!svc)
            return MICROUDS_ERR_MEMORY;

        svc->func = table[i].func;
        svc->param = table[i].param;
        svc->sid = table[i].sid;
        svc->Session = NULL;

        if (MicroHash_Insert(&MicroUDS_Handle->hashTable, (MicroHash_key_t)table[i].sid, (void *)svc) != MICROHASH_OK)
        {
            free(svc);
            return MICROUDS_ERR_HASH;
        }

        if (MicroUDS_Handle->Record.count < MicroUDS_Handle->Record.size)
            MicroUDS_Handle->Record.data[MicroUDS_Handle->Record.count++] = (uint8_t)table[i].sid;
    }

    return MICROUDS_OK;
}

MicroUDS_Sta_t MicroUDS_RegisterSession(MicroUDS_Sid_t sid, MicroUDS_SessionTable_t *table, size_t table_len)
{
    MICROUDS_CHECKPTR(table);
    if (table_len == 0)
        return MICROUDS_ERR_PARAM;

    Microuds_Service_t *svc = (Microuds_Service_t *)MicroHash_Find(&MicroUDS_Handle->hashTable, (MicroHash_key_t)sid);
    if (!svc)
        return MICROUDS_ERR_PARAM;

    MicroUDS_Session_t *last = svc->Session;
    while (last && last->next)
        last = last->next;

    for (size_t i = 0; i < table_len; i++)
    {
        MicroUDS_Session_t *node = (MicroUDS_Session_t *)calloc(1, sizeof(MicroUDS_Session_t));
        if (!node)
            return MICROUDS_ERR_MEMORY;

        node->ssid = table[i].ssid;
        node->param = table[i].param;
        node->func = table[i].func;
        node->next = NULL;

        if (!svc->Session)
        {
            svc->Session = node;
            last = node;
        }
        else
        {
            last->next = node;
            last = node;
        }
    }
    return MICROUDS_OK;
}

void MicroUDS_TickHandler(void)
{
    MicroUDS_Handle->Tick++;
}

void MicroUDS_ResetTimer(void)
{
    MicroUDS_Handle->last_time = MicroUDS_Handle->Tick;
}

void MicroUDS_TimerHandler(void)
{
    uint32_t current_time = MicroUDS_Handle->Tick;

    if (current_time - MicroUDS_Handle->last_time >= MicroUDS_Handle->Timeout)
    {
        MicroUDS_Handle->last_time = current_time;

        MicroUDS_Handle->sid = UDS_DIAGNOSTIC_SESSION_CONTROL;
        MicroUDS_Handle->ssid = UDS_SESSION_DEFAULT;
        return;
    }

    if (MicroUDS_Handle->N_Cs.Active)
    {
        MicroUDS_Handle->N_Cs.tick = MicroUDS_Handle->Tick; // N_CS定时器
        if (MicroUDS_Handle->N_Cs.tick - MicroUDS_Handle->N_Cs.lash_tick >= MICROUDS_SERVICE_TIMEOUT_MS)
        {
            MicroUDS_Handle->N_Cs.lash_tick = MicroUDS_Handle->N_Cs.tick;
            // 多帧超时
            memset(&MicroUDS_Handle->MultiFrame, 0, sizeof(MicroUDS_MultiFrame_t));
            MicroUDS_Handle->N_Cs.Active = false;
        }
    }
    if (MicroUDS_Handle->active == UDS_ACTIVE_NO)
    {
        return; // 没有请求
    }
    MicroUDS_Handle->active = UDS_ACTIVE_NO;

    Microuds_Service_t *svc = (Microuds_Service_t *)MicroHash_Find(&MicroUDS_Handle->hashTable, (MicroHash_key_t)MicroUDS_Handle->sid); // 找服务
    if (!svc)
    {

        MicroUDS_NegativeResponse(UDS_NRC_SERVICE_NOT_SUPPORTED);
        MicroUDS_ClearRecv();
        return;
    }

    if (svc->func)
    {
        UDS_ECUSETBUSY(); // ECU置忙
        MicroUDS_NRC_t ret = svc->func(svc->param);
        MicroUDS_Response(ret);
        UDS_ECUCLEAR(); // ECU清除忙等待
    }

    bool session_found = false;
    for (MicroUDS_Session_t *ses = svc->Session; ses; ses = ses->next)
    {
        if (ses->ssid == MicroUDS_Handle->ssid)
        {
            session_found = true;
            if (ses->func)
            {
                UDS_ECUSETBUSY();
                MicroUDS_NRC_t ret = ses->func(ses->param);
                MicroUDS_Response(ret);
                UDS_ECUCLEAR();
            }

            break;
        }
    }

    if (!session_found)
    {
        MicroUDS_NegativeResponse(UDS_NRC_SUBFUNCTION_NOT_SUPPORTED);
    }

    MicroUDS_ClearRecv();
}

static void MicroUDS_ClearRecv(void)
{
    memset(&MicroUDS_Handle->Recbuf, 0, sizeof(MicroUDS_Isotp_t));

    memset(&MicroUDS_Handle->MultiFrame, 0, sizeof(MicroUDS_MultiFrame_t));
}

void MicroUDS_ReceiveCallback(uint8_t *data)
{
    if (data == NULL)
        return;

    Isotp_FrameType_t FrameType = (Isotp_FrameType_t)((data[0] & 0xF0) >> 4);

    switch (FrameType)
    {
    case FRAME_SINGLE:

        if (Isotp_UnpackSingleFrame(&MicroUDS_Handle->Recbuf.SF, data) != ISOTP_OK)
            return;

        UDS_CHECK_ECU_BUSY(); // 检查ECU是否忙
        MicroUDS_ResetTimer();
        MicroUDS_Handle->sid = MicroUDS_Handle->Recbuf.SF.byte.Payload[0];
        MicroUDS_Handle->ssid = MicroUDS_Handle->Recbuf.SF.byte.Payload[1];
        MicroUDS_Handle->active = UDS_ACTIVE_SIGNAL;
        break;

    case FRAME_FIRST: // 首帧
    {
        if (Isotp_UnpackFirstFrame(&MicroUDS_Handle->Recbuf.FF, data) != ISOTP_OK)
            return;

        if (MicroUDS_Handle->MultiFrame.receiving)
        {
            MicroUDS_NegativeResponse(UDS_NRC_REQUEST_SEQ_ERROR);
            memset(&MicroUDS_Handle->MultiFrame, 0, sizeof(MicroUDS_MultiFrame_t));
        }

        UDS_CHECK_ECU_BUSY(); // 检查ECU是否忙
        MicroUDS_ResetTimer();

        MicroUDS_Handle->MultiFrame.total_len =
            ((MicroUDS_Handle->Recbuf.FF.byte.FF_DL_H & 0x0F) << 8) |
            MicroUDS_Handle->Recbuf.FF.byte.FF_DL_L;

        /* 边界检查：避免超过 buf 长度 */
        if (MicroUDS_Handle->MultiFrame.total_len > sizeof(MicroUDS_Handle->MultiFrame.buf))
        {
            /* 总长度超限，拒绝或截断，根据策略返回 overflow */
            MicroUDS_NegativeResponse(UDS_NRC_RESPONSE_TOO_LONG);
            memset(&MicroUDS_Handle->MultiFrame, 0, sizeof(MicroUDS_MultiFrame_t));
            break;
        }

        size_t ff_payload = 6;
        if (ff_payload > MicroUDS_Handle->MultiFrame.total_len)
            ff_payload = MicroUDS_Handle->MultiFrame.total_len;

        memcpy(MicroUDS_Handle->MultiFrame.buf,
               MicroUDS_Handle->Recbuf.FF.byte.Payload, ff_payload);

        MicroUDS_Handle->MultiFrame.recv_len = (uint16_t)ff_payload;
        MicroUDS_Handle->MultiFrame.next_sn = 1;
        MicroUDS_Handle->MultiFrame.receiving = true;

        if (MicroUDS_Handle->MultiFrame.recv_len >= 1)
            MicroUDS_Handle->sid = MicroUDS_Handle->MultiFrame.buf[0];
        // if (MicroUDS_Handle->MultiFrame.recv_len >= 2)
        // MicroUDS_Handle->ssid = MicroUDS_Handle->MultiFrame.buf[1];

        Isotp_PackFlowControlFrame(MicroUDS_Handle->Recbuf.FC.data, 0, 0, ISOTP_FS_CTS);
        if (MicroUDS_Handle->Transmit)
            MicroUDS_Handle->Transmit(MicroUDS_Handle->Recbuf.FC.data, 8);

        MicroUDS_Handle->N_Cs.Active = true;
    }
    break;

    case FRAME_CONSECUTIVE:
    {
        if (!MicroUDS_Handle->MultiFrame.receiving)
            break;

        if (Isotp_UnPackConsecutiveFrame(&MicroUDS_Handle->Recbuf.CF, data) != ISOTP_OK)
        {
            memset(&MicroUDS_Handle->MultiFrame, 0, sizeof(MicroUDS_MultiFrame_t));
            return;
        }

        MicroUDS_ResetTimer();
        uint8_t sn = MicroUDS_Handle->Recbuf.CF.byte.SN;
        if (sn != MicroUDS_Handle->MultiFrame.next_sn)
        {
            MicroUDS_Handle->MultiFrame.receiving = false;
            MicroUDS_NegativeResponse(UDS_NRC_REQUEST_SEQ_ERROR);
            memset(&MicroUDS_Handle->MultiFrame, 0, sizeof(MicroUDS_MultiFrame_t));
            break;
        }

        size_t remaining = MicroUDS_Handle->MultiFrame.total_len - MicroUDS_Handle->MultiFrame.recv_len;
        size_t copy_len = remaining >= 7 ? 7 : remaining;

        memcpy(MicroUDS_Handle->MultiFrame.buf + MicroUDS_Handle->MultiFrame.recv_len,
               MicroUDS_Handle->Recbuf.CF.byte.Payload, copy_len);

        MicroUDS_Handle->MultiFrame.recv_len += (uint16_t)copy_len;
        MicroUDS_Handle->MultiFrame.next_sn = (uint8_t)((sn + 1) & 0x0F);

        if (MicroUDS_Handle->MultiFrame.recv_len >= MicroUDS_Handle->MultiFrame.total_len)
        {
            MicroUDS_Handle->MultiFrame.receiving = false;
            MicroUDS_Handle->active = UDS_ACTIVE_MULTI;

            memset((void*)&MicroUDS_Handle->N_Cs, 0, sizeof(MicroUDS_N_Cs_t));
        }
    }
    break;

    case FRAME_FLOWCONTROL:
        /* ECU 不处理流控帧*/
        break;

    default:
        /* 未知帧类型忽略 */
        break;
    }
}
static inline void MicroUDS_Response(MicroUDS_NRC_t code)
{
    switch (code)
    {
    case UDS_NRC_SUCCESS: // 正响应
        MicroUDS_PositiveResponse();
        break;
    case UDS_NRC_NO: // 直接跳过，不做响应 (默认)
        break;
    default:
        MicroUDS_NegativeResponse(code); // 负响应
        break;
    }
}

MicroUDS_Sta_t MicroUDS_ReadMultiframeInfo(MicroUDS_MultiInfo_t *info)
{
    MICROUDS_CHECKPTR(info);

    if (MicroUDS_Handle->MultiFrame.recv_len <= 1)
        return MICROUDS_ERR;

    info->sid = MicroUDS_Handle->MultiFrame.buf[0];
    info->data = &MicroUDS_Handle->MultiFrame.buf[1];
    info->data_len = MicroUDS_Handle->MultiFrame.recv_len - 1;

    return MICROUDS_OK;
}

uint32_t MicroUDS_GetTickCount()
{
    return MicroUDS_Handle->Tick;
}

/* EOF */
