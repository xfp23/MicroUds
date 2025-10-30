#include "Microuds.h"
#include "Microuds_com.h"

static MicroUDS_Obj MicroUDS = {0};

const MicroUDS_Handle_t MicroUDS_Handle = &MicroUDS;

static inline int PositiveResponse()
{
    
    return 0;
}

static inline NegativeResponse(MicroUds_NegativeResponseCode_t code)
{

}

void MicroUDS_Tickhandler()
{
    MicroUDS_Handle->Tick++;
}

MicroUDS_Sta_t MicroUDS_Init(MicroUDS_Conf_t *conf)
{
    MICROUDS_CHECKPTR(conf);
    if (conf->MemorySize == 0)
        return MICROUDS_ERR_PARAM;

    MicroHash_Conf_t hashConf = {
        .buckSize = conf->MemorySize,
    };

    MicroUDS_Handle->Transmit = conf->Transmit;
    MicroUDS_Handle->Receive = conf->Reveive;

    MicroUDS_Handle->Record.data = (uint8_t *)calloc(conf->RecordSize, sizeof(uint8_t));

    if (MicroUDS_Handle->Record.data == NULL)
    {
        return MICROUDS_ERR_MEMORY;
    }
    MicroHash_Sta_t HashRet = MicroHash_Init(&MicroUDS_Handle->hashTable, &hashConf);

    if (HashRet != MICROHASH_OK)
    {
        switch (HashRet)
        {
        case MICROHASH_ERR:
            return MICROUDS_ERR;
            break;
        case MICROHASH_ERR_PARAM:
            return MICROUDS_ERR_HASH;
        case MICROHASH_ERR_MEMORY:
            return MICROUDS_ERR_MEMORY;
        default:
            return MICROUDS_ERR;
            break;
        }
    }

    return MICROUDS_OK;
}

MicroUDS_Sta_t MicroUDS_RegisterService(MicroUDS_ServiceTable_t *table, size_t table_len)
{
    MICROUDS_CHECKPTR(table);
    if (table_len == 0)
        return MICROUDS_ERR_PARAM;

    for (size_t i = 0; i < table_len; i++)
    {
        Microuds_Service_t *svc = (Microuds_Service_t*)calloc(1, sizeof(Microuds_Service_t));
        if (!svc)
            return MICROUDS_ERR_MEMORY;

        svc->func = table[i].func;
        svc->param = table[i].param;
        svc->sid = table[i].sid;
        svc->Session = NULL;

        if (MicroHash_Insert(&MicroUDS_Handle->hashTable, table[i].sid, (void*)svc) != MICROHASH_OK)
        {
            free(svc);
            return MICROUDS_ERR_HASH;
        }

        MicroUDS_Handle->Record.data[MicroUDS_Handle->Record.count++] = table[i].sid;
    }

    return MICROUDS_OK;
}


MicroUDS_Sta_t MicroUDS_RegisterSession(MicroUDS_Sid_t sid, MicroUDS_SessionTable_t *table, size_t table_len)
{
    MICROUDS_CHECKPTR(table);
    if (table_len == 0)
        return MICROUDS_ERR_PARAM;

    Microuds_Service_t *svc = (Microuds_Service_t *)MicroHash_Find(MicroUDS_Handle->hashTable, (MicroHash_key_t)sid);
    if (!svc)
        return MICROUDS_ERR_PARAM;

    MicroUDS_Session_t *last = svc->Session;

    while (last && last->next)
        last = last->next;

    for (int i = 0; i < table_len; i++)
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

void MicroUDS_TimerHandler()
{
    if(MicroUDS_Handle->Recbuf.SF.data[0] != 0 || MicroUDS_Handle->Recbuf.FF.data[0] != 0) // 单帧和首帧切换服务
      {
        Microuds_Service_t *svc = (Microuds_Service_t*)MicroHash_Find(MicroUDS_Handle->hashTable,MicroUDS_Handle->sid); // 找服务

        if(svc == NULL)
        return;

        if(svc->func != NULL)
        {
        svc->func(svc->param);
        } else {
            NegativeResponse(UDS_NRC_SERVICE_NOT_SUPPORTED);
            goto clear_buf;
        }

        
         MicroUDS_Session_t *ses = svc->Session;
         while (ses) // 找子功能
         {
            if(ses->ssid == MicroUDS_Handle->ssid && ses->func != NULL)
            {
                ses->func(ses->param); // 执行
                break;
            }
            ses = ses->next;
         }

clear_buf:
         memset((void*)&MicroUDS_Handle->Recbuf,0,sizeof(MicroUDS_Isotp_t)); // 清空buffer
      }
    

    

}

void MicroUDS_ReceiveCallback(uint8_t *data)
{
    if (data == NULL)
        return;

    Isotp_FrameType_t FrameType = (data[0] & 0xF0) >> 4;

    switch (FrameType)
    {
    case FRAME_SINGLE:
        Isotp_UnpackSingleFrame(&MicroUDS_Handle->Recbuf.SF.data, data);
        MicroUDS_Handle->sid = MicroUDS_Handle->Recbuf.SF.byte.Payload[0];
        MicroUDS_Handle->ssid = MicroUDS_Handle->Recbuf.SF.byte.Payload[1];
        break;

    case FRAME_FIRST:
        Isotp_UnpackFirstFrame(&MicroUDS_Handle->Recbuf.FF.data, data);
        MicroUDS_Handle->sid = MicroUDS_Handle->Recbuf.FF.byte.Payload[0];
        break;

    case FRAME_CONSECUTIVE:
        Isotp_UnPackConsecutiveFrame(&MicroUDS_Handle->Recbuf.CF.data, data);
        break;

    case FRAME_FLOWCONTROL:
        Isotp_UnpackFlowControlFrame(&MicroUDS_Handle->Recbuf.FC.data, data);
        break;
    }
}

void MicroUDS_Delete()
{
}