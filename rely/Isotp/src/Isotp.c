#include "Isotp.h"

static Isotp_Obj Isotp = {0};

const Isotp_Handle_t Isotp_Handle = &Isotp;

Isotp_Sta_t Isotp_PackSingleFrame(uint8_t *Dst, const uint8_t *Src, size_t size)
{
    if (Dst == NULL || Src == NULL)
        return ISOTP_ERR_PARAM;

    if (size == 0 || size > 7)
        return ISOTP_ERR_LENGTH;

    memset(Isotp_Handle->SF.data, 0, sizeof(Isotp_Handle->SF.data));

    Isotp_Handle->SF.byte.PCIType = FRAME_SINGLE;
    Isotp_Handle->SF.byte.PCI_DL = (uint8_t)(size & 0x0F);
    memcpy(Isotp_Handle->SF.byte.Payload, Src, size);
    memcpy(Dst, Isotp_Handle->SF.data, 8);

    return ISOTP_OK;
}

Isotp_Sta_t Isotp_UnpackSingleFrame(Isotp_SingleFrame_t *Dst, const uint8_t *Src)
{
    if (!Dst || !Src)
        return ISOTP_ERR_PARAM;

    memcpy(Dst->data, Src, 8);

    if (Dst->byte.PCIType != FRAME_SINGLE)
    {
        memset(Dst->data, 0, 8);
        return ISOTP_ERR_TYPE;
    }

    if (Dst->byte.PCI_DL > 7)
    {
        memset(Dst->data, 0, 8);
        return ISOTP_ERR_LENGTH;
    }

    return ISOTP_OK;
}

Isotp_Sta_t Isotp_PackFirstFrame(uint8_t *Dst, const uint8_t *Src, size_t size)
{
    if (Dst == NULL || Src == NULL)
        return ISOTP_ERR_PARAM;

    if (size <= 7 || size > 0xFFF) // FF 只用于 >7 字节的情况
        return ISOTP_ERR_LENGTH;

    memset(Isotp_Handle->FF.data, 0, sizeof(Isotp_Handle->FF.data));

    Isotp_Handle->FF.byte.PCIType = FRAME_FIRST;
    Isotp_Handle->FF.byte.FF_DL_H = (size >> 8) & 0x0F;
    Isotp_Handle->FF.byte.FF_DL_L = size & 0xFF;

    memcpy(Isotp_Handle->FF.byte.Payload, Src, 6);
    memcpy(Dst, Isotp_Handle->FF.data, 8);

    return ISOTP_OK;
}

Isotp_Sta_t Isotp_UnpackFirstFrame(Isotp_FirstFrame_t *Dst, const uint8_t *Src)
{
    if (Dst == NULL || Src == NULL)
        return ISOTP_ERR_PARAM;

    memcpy(Dst->data, Src, 8);

    if (Dst->byte.PCIType != FRAME_FIRST)
    {
        memset(Dst->data, 0, 8);
        return ISOTP_ERR_TYPE;
    }

    return ISOTP_OK;
}

Isotp_Sta_t Isotp_PackFlowControlFrame(uint8_t *Dst, uint8_t bs, uint8_t STime, Isotp_FlowStatus_t fs)
{
    if (Dst == NULL)
        return ISOTP_ERR_PARAM;

    memset(Isotp_Handle->FC.data, 0, 8);
    Isotp_Handle->FC.byte.PCIType = FRAME_FLOWCONTROL;
    Isotp_Handle->FC.byte.BS = bs;
    Isotp_Handle->FC.byte.FS = fs;
    Isotp_Handle->FC.byte.STmin = STime;
    memcpy(Dst, Isotp_Handle->FC.data, 8);

    return ISOTP_OK;
}

Isotp_Sta_t Isotp_UnpackFlowControlFrame(Isotp_FlowControlFrame_t *Dst, uint8_t *Src)
{
    if (Dst == NULL)
        return ISOTP_ERR_PARAM;

    memcpy(Dst->data, Src, 8);
    if (Dst->byte.PCIType != FRAME_FLOWCONTROL)
    {
        memset(Dst->data, 0, 8);
        return ISOTP_ERR_TYPE;
    }

    return ISOTP_OK;
}

Isotp_Sta_t Isotp_PackConsecutiveFrame(uint8_t *Dst, uint8_t *Src, size_t size, uint8_t SN)
{
    if (Dst == NULL || Src == NULL)
        return ISOTP_ERR_PARAM;

    if (size == 0 || size > 7)
        return ISOTP_ERR_LENGTH;

    if (SN > 0x0F)
        return ISOTP_ERR_PARAM;
        
    memset(Isotp_Handle->CF.data, 0, 8);

    Isotp_Handle->CF.byte.PCIType = FRAME_CONSECUTIVE;
    Isotp_Handle->CF.byte.SN = SN;
    memcpy(Isotp_Handle->CF.byte.Payload, Src, size);

    memcpy(Dst, Isotp_Handle->CF.data, 8);
    return ISOTP_OK;
}

Isotp_Sta_t Isotp_UnPackConsecutiveFrame(Isotp_ConsecutiveFrame_t *Dst, uint8_t *Src)
{
    if (Dst == NULL || Src == NULL)
        return ISOTP_ERR_PARAM;

    memcpy(Dst->data, Src, 8);

    if (Dst->byte.PCIType != FRAME_CONSECUTIVE)
    {
        memset(Dst->data, 0, 8);
        return ISOTP_ERR_TYPE;
    }

    if (Dst->byte.SN > 0x0F)
    {
        memset(Dst->data, 0, 8);
        return ISOTP_ERR_FRAME;
    }

    return ISOTP_OK;
}

// Isotp_Sta_t Isotp_UnpackFrame(uint8_t *Dst,uint8_t *Src,size_t size)
// {
//     if (Dst == NULL || Src == NULL)
//         return ISOTP_ERR_PARAM;

//     Isotp_FrameType_t FrameType = (Src[0] & 0xF0) >> 4;
//     switch(FrameType)
//     {
//     case FRAME_SINGLE:
//         Isotp_UnpackSingleFrame(Dst,Src);
//     break;
//     }
// }