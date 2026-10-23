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

    uint16_t did_size = ctx->req_len; // 子功能长度 + 请求数据
    uint16_t total_len = 0;
    uint16_t last_len = 0;

    if ((did_size % 2) != 0)
    {
        MicroUds_CtxNegative(ctx, MICROUDS_NRC_IMLOI);
        return; // 返回负响应
    }

    uint16_t did_len = (uint16_t)(did_size * 0.5f); // / 2

    // 先找一下动态did，再去翻静态did
    bool is_dync = false;

    for (uint16_t i = 0; i < did_len; i++)
    {
        uint16_t did = (uint16_t)(ctx->req[i] << 8) | (ctx->req[i + 1]);

        MicroUdsDid_Dynamically_t *dyna_item = MicroUds_Did_FindDynamic(did);

        if (dyna_item == NULL)
        {

            continue;
        }

        is_dync = true;
        // 按did
        // 因为动态did是组合的静态did，所以我应该保留回调？

        for (uint16_t i = 0; i < dyna_item->item_num; i++)
        {
            if (dyna_item->type == MICROUDSDID_DYNA_ID)
            {
                MicroUdsDid_Ctx_t didctx = {0};

                didctx.data = dyna_item->Item[i].data;
                didctx.data_len = dyna_item->Item[i].data_len;
                didctx.Userdata = dyna_item->Item[i].Userdata;
                didctx.Did = dyna_item->Item[i].Identifier;
                last_len = dyna_item->Item[i].data_len;
                total_len += dyna_item->Item[i].data_len;

                if (dyna_item->Item[i].cb != NULL)
                {
                    dyna_item->Item[i].cb(&didctx);
                }

                if (didctx.nrc != 0)
                {
                    MicroUds_CtxNegative(ctx, didctx.nrc);
                    return; // 可能是pedding啥的吧
                }
            }

            memcpy(ctx->payload + last_len, dyna_item->Item[i].data, dyna_item->Item[i].data_len);
        }
    }

    if (is_dync)
    {
        MicroUds_CtxPositive(ctx, total_len);
        return;
    }

    for (uint16_t i = 0; i < did_len; i++)
    {
        uint16_t did = (uint16_t)(ctx->req[i] << 8) | (ctx->req[i + 1]);

        MicroUdsDid_Dynamically_t *dyna_item = MicroUds_Did_FindDynamic(did);

        MicroUdsDidItem_t *did_item = MicroUds_Did_FindReadIdentifier(did);
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

        if (did_item->cb != NULL)
        {
            did_item->cb(&didctx);
        }

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

    uint16_t did = ((uint16_t)(ctx->req[0] << 8) | ctx->req[1]); // 组合did

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

MicroUdsDid_Dynamically_t *MicroUds_Did_FindDynamic(uint16_t id)
{
    for (uint16_t i = 0; i < MICROUDS_DID_DYNAMICALLY_SIZE; i++)
    {
        if (did_obj.Dynamically[i].is_active && did_obj.Dynamically[i].dync_did == id)
        {
            return &did_obj.Dynamically[i];
        }
    }

    return NULL;
}

// DynamicallyDefineDataIdentifier 01 02 03 内核注册为没有子功能，内核不去解析，我们自己解析
void MicroUds_Did_0x2CDynamicHandler(MicroUds_ServiceCtx_t *ctx)
{
    if (ctx->req_len < 5)
    {
        MicroUds_CtxNegative(ctx, MICROUDS_NRC_IMLOI);
        return;
    }

    uint8_t sub_func = ctx->req[1];

    uint16_t dync_did = (uint16_t)(ctx->req[2] << 8) | ctx->req[3];
    // uint16_t src_did = (uint16_t)(ctx->req[4] << 8) | ctx->req[5];

    MicroUdsDid_Dynamically_t *dyna_itme = MicroUds_Did_FindDynamic(dync_did);

    if (dyna_itme != NULL)
    {
        // 已经存在了，得清除一次再去定义
        memset(dyna_itme, 0, sizeof(MicroUdsDid_Dynamically_t)); // 清除后重新定义
        did_obj.dync_num--;
    }

    dyna_itme = &did_obj.Dynamically[did_obj.dync_num]; // 获取空闲位置
    // 动态标识符定义
    switch (ctx->sub_func)
    {
    // [ask] 我的理解没问题吧 请求帧: [2c 01] [F2 00](这是动态标识符) [10 01](源DID) [01](起始位置) [02](长度) * N
    case 0x01: // 定义动态did
    {
        uint16_t did_num = (ctx->req_len - 3) / 4;

        if (did_num > MICROUDS_DID_DYNAMICALLY_ITEM_LEN)
        {
            MicroUds_CtxNegative(ctx, MICROUDS_NRC_IMLOI);
            return;
        }
        for (uint16_t i = 0; i < did_num; i++)
        {
            uint16_t src_did = (uint16_t)(ctx->req[i + 3] << 8) | ctx->req[i + 4];
            MicroUdsDidItem_t *static_item = MicroUds_Did_FindReadIdentifier(src_did);

            if (static_item == NULL)
            {
                memset(dyna_itme->Item, 0, sizeof(MicroUdsDidItem_t) * MICROUDS_DID_DYNAMICALLY_ITEM_LEN);
                MicroUds_CtxNegative(ctx, MICROUDS_NRC_ROOR);
                break; // 返回负响应
            }
            dyna_itme->Item[i] = *static_item;
        }
        did_obj.dync_num++;
        dyna_itme->type = MICROUDSDID_DYNA_ID;
        dyna_itme->item_num = did_num;
        memcpy(ctx->payload, (uint8_t *)dync_did, 2);
        MicroUds_CtxPositive(ctx, 2); // 返回正响应
    }
    break;

    case 0x02: // 按照地址定义
    {
        // [2c 02] [F2 00] [24](ALFID 表示地址4字节，长度两字节) [20 00 10 00](内存地址) [00 02](长度) *N

        // 对于ARM 32位单片机，不是4字节直接返回错误就好了吧?

        uint32_t addr_num = (ctx->req_len - 4) / ((ctx->req[3] & 0x0F) + ctx->req[3] >> 4); // 总字节长度 / 单个长度

        for (uint32_t i = 0; i < addr_num; i++)
        {
            uint32_t addr = ctx->req[i + 4] << 24 | ctx->req[i + 5] << 16 | ctx->req[i + 6] << 8 | ctx->req[i + 7];
            uint16_t len = ctx->req[i + 8] << 8 | ctx->req[i + 9];

            if (addr == 0)
            {
                memset(dyna_itme->Item, 0, sizeof(MicroUdsDidItem_t) * MICROUDS_DID_DYNAMICALLY_ITEM_LEN);
                MicroUds_CtxNegative(ctx, MICROUDS_NRC_ROOR);
                break; // 返回负响应
            }

            dyna_itme->Item[i].data = (uint8_t *)addr;
            dyna_itme->Item[i].data_len = len;
        }

        did_obj.dync_num++;
        dyna_itme->is_active = true;
        dyna_itme->type = MICROUDSDID_DYNA_ADDRESS;
        dyna_itme->item_num = addr_num;
        memcpy(ctx->payload, (uint8_t *)dync_did, 2);
        MicroUds_CtxPositive(ctx, 2); // 返回正响应
        // dyna_itme.
    }
    break;

    case 0x03: // 清除动态
        memset(did_obj.Dynamically, 0, sizeof(MicroUdsDid_Dynamically_t) * MICROUDS_DID_DYNAMICALLY_SIZE);
        break;
    }
}

#endif
