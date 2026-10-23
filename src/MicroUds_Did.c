/**
 * @file MicroUds_Did.c
 * @author Xfp23
 * @brief 先只实现 22 2E吧
 * @version 0.1
 * @date 2026-10-23
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "MicroUds_Did.h"
#include "MicroUds.h"

#if MICROUDS_DID_ENABLE

static MicroUdsDid_Obj_t did_obj = {0};

// static MicroUds_Obj_t const *did_ptr = &did_obj; // 用指针是为了以后的内存管理做准备,预留，到时候再说吧

static bool MicroUds_Did_SecurityPass(uint8_t sercurity_level, uint8_t permis)
{
    uint8_t need = MICROUDS_PERMIS_LEVEL(permis);

    if (need == 0u)
    {
        return true; // 无需解锁
    }

    if (MICROUDS_GET_ASSIG(permis))
    {
        return sercurity_level == need; // 指定位: 只有恰好这个等级才允许
    }

    return sercurity_level >= need; // 已解锁等级 >= 要求等级
}

MicroUds_Status_t MicroUds_Did_Init(void)
{
    memset(&did_obj, 0, sizeof(MicroUdsDid_Obj_t));

    return MICROUDS_OK;
}

// 只能注册一个表，不允许附加
MicroUds_Status_t MicroUds_Did_RegisterReadItem(MicroUdsDidItem_Config_t *table, size_t size)
{
    if (table == NULL || size == 0)
    {
        return MICROUDS_PARAM_ERR;
    }

    for (size_t i = 0; i < MICROUDS_DID_READ_SIZE; i++)
    {
        MicroUdsDidItem_t *item = &did_obj.Read[i];
        item->cb = table[i].cb;
        item->data = table[i].data;
        item->data_len = table[i].data_len;
        item->Identifier = table[i].Identifier;
        item->permis = table[i].permis;
        item->session = table[i].session;
        item->Userdata = table[i].Userdata;

        item->is_vaild = true;
    }

    did_obj.read_num = size;

    return MICROUDS_OK;
}

static MicroUdsDidItem_t *MicroUds_Did_FindReadIdentifier(uint16_t Identifier);

// 22服务回调
void MicroUds_Did_0x22ReadHandler(MicroUds_ServiceCtx_t *ctx)
{
    // 请求长度 + 1 就算是did长度了吧

    uint16_t did_size = ctx->req_len + 1; // 子功能长度 + 请求数据
    uint16_t total_len = 0;
    uint16_t last_len = 0;

    if ((did_size % 2) != 0)
    {
        MicroUds_CtxNegative(ctx, MICROUDS_NRC_IMLOI);
        return; // 返回负响应
    }

    uint16_t did_len = (uint16_t)(did_size * 0.5f); // / 2
    uint16_t did = (ctx->sid << 8) | (ctx->req[0]); // 获取第一个did

    MicroUdsDidItem_t *did_item = MicroUds_Did_FindReadIdentifier(did);

    if (did_item != NULL)
    {

        if (did_item->session != 0 && (ctx->session & did_item->session) == 0)
        {
            MicroUds_CtxNegative(ctx, MICROUDS_NRC_CNC); // 会话下不满足
            return;
        }

        if (MicroUds_Did_SecurityPass(ctx->sec_level, did_item->permis))
        {
            MicroUds_CtxNegative(ctx, MICROUDS_NRC_SAD); // 安全访问拒绝
            return;
        }

        if (ctx->payload == NULL)
        {
            MicroUds_CtxNegative(ctx, MICROUDS_NRC_GR);
            return;
        }

        if (ctx->payload_cap < did_item->data_len)
        {
            MicroUds_CtxNegative(ctx, MICROUDS_NRC_RTL);
            return;
        }

        MicroUdsDid_Ctx_t didctx = {0};

        didctx.data = did_item->data;
        didctx.data_len = did_item->data_len;
        didctx.Userdata = did_item->Userdata;
        didctx.Did = did_item->Identifier;
        last_len = did_item->data_len;
        total_len += did_item->data_len;

        did_item->cb(&didctx);

        if (didctx.nrc != 0)
        {
            MicroUds_CtxNegative(ctx, didctx.nrc);
            return; // 可能是pedding啥的吧
        }
        memcpy(ctx->payload, did_item->data, did_item->data_len); // 移动数据
        // MicroUds_CtxPositive(ctx,did_item->data_len); // 这里还不能响应，应该继续往下找
    }

    for (uint16_t i = 0; i < did_len; i++)
    {
        did = (uint16_t)(ctx->req[i + 1] << 8) | (ctx->req[i + 2]);
        did_item = MicroUds_Did_FindReadIdentifier(did);

        if (did_item == NULL)
        {
            MicroUds_CtxNegative(ctx, MICROUDS_NRC_ROOR);
            return;
        }

        if (did_item->session != 0 && (ctx->session & did_item->session) == 0)
        {
            MicroUds_CtxNegative(ctx, MICROUDS_NRC_CNC); // 会话下不满足
            return;
        }

        if (MicroUds_Did_SecurityPass(ctx->sec_level, did_item->permis))
        {
            MicroUds_CtxNegative(ctx, MICROUDS_NRC_SAD); // 安全访问拒绝
            return;
        }

        if (ctx->payload_cap < did_item->data_len)
        {
            MicroUds_CtxNegative(ctx, MICROUDS_NRC_RTL);
            return;
        }

        MicroUdsDid_Ctx_t didctx = {0};

        didctx.data = did_item->data;
        didctx.data_len = did_item->data_len;
        didctx.Userdata = did_item->Userdata;
        didctx.Did = did_item->Identifier;
        last_len = did_item->data_len;
        total_len += did_item->data_len;

        did_item->cb(&didctx);

        if (didctx.nrc != 0)
        {
            MicroUds_CtxNegative(ctx, didctx.nrc);
            return; // 可能是pedding啥的吧
        }

        memcpy(ctx->payload + last_len, did_item->data, did_item->data_len);
        last_len = did_item->data_len;
    }

    MicroUds_CtxPositive(ctx, total_len);
}

static MicroUdsDidItem_t *MicroUds_Did_FindReadIdentifier(uint16_t Identifier)
{
    for (uint16_t i = 0; i < did_obj.read_num; i++)
    {
        if (Identifier == did_obj.Read[i].Identifier)
        {
            if (!did_obj.Read[i].is_vaild)
            {
                return NULL;
            }
            return &did_obj.Read[i];
        }
    }

    return NULL;
}

static MicroUdsDidItem_t *MicroUds_Did_FindWriteIdentifier(uint16_t Identifier)
{
    for (uint16_t i = 0; i < did_obj.write_num; i++)
    {
        if (Identifier == did_obj.Write[i].Identifier)
        {
            if (!did_obj.Write[i].is_vaild)
            {
                return NULL;
            }
            return &did_obj.Write[i];
        }
    }

    return NULL;
}

MicroUds_Status_t MicroUds_Did_RegisterWriteItem(MicroUdsDidItem_Config_t *table, size_t size)
{
    if (table == NULL || size == 0)
    {
        return MICROUDS_PARAM_ERR;
    }

    for (size_t i = 0; i < MICROUDS_DID_WRITE_SIZE; i++)
    {
        MicroUdsDidItem_t *item = &did_obj.Write[i];
        item->cb = table[i].cb;
        item->data = table[i].data;
        item->data_len = table[i].data_len;
        item->Identifier = table[i].Identifier;
        item->permis = table[i].permis;
        item->session = table[i].session;
        item->Userdata = table[i].Userdata;

        item->is_vaild = true;
    }

    did_obj.write_num = size;

    return MICROUDS_OK;
}

void MicroUds_Did_0x2EWriteHandler(MicroUds_ServiceCtx_t *ctx)
{
    uint16_t did_len = ctx->req_len - 3;

    if (did_len % 2 != 0)
    {
        MicroUds_CtxNegative(ctx, MICROUDS_NRC_IMLOI);
        return;
    }

    uint16_t did = ((uint16_t)(ctx->sid << 8) | ctx->req[0]); // 组合did

    MicroUdsDidItem_t *Item = MicroUds_Did_FindWriteIdentifier(did);

    if (Item == NULL)
    {
        MicroUds_CtxNegative(ctx, MICROUDS_NRC_ROOR);
        return;
    }

    // if(did_len > Item->data_len)
    // {
    // 这里应该是负响应吗
    // }
    memcpy(Item->data, ctx->req, ctx->req_len); // 执行写入
    MicroUdsDid_Ctx_t didctx = {0};
    didctx.Did = Item->Identifier;
    didctx.Did_len = 1;
    didctx.Userdata = Item->Userdata;
    didctx.data = Item->data;
    didctx.data_len = Item->data_len;
    Item->cb(&didctx);

    if (didctx.nrc != 0)
    {
        MicroUds_CtxNegative(ctx, didctx.nrc);
        return;
    }

    MicroUds_CtxPositive(ctx, Item->data_len);
}
#endif
