/**
 * @file MicroUds_Did.c
 * @author Xfp23
 * @brief 先只实现 22 2E 2C也写了吧
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

static MicroUdsDidItem_t *MicroUds_Did_FindReadIdentifier(uint16_t Identifier);
static MicroUdsDidItem_t *MicroUds_Did_FindWriteIdentifier(uint16_t Identifier);

// [REVIEW] 单个响应里一个DID最多能装多少数据: 响应缓冲 - RSID(1) - DID(2)。读表项、动态DID的长度都不能超过它
#define MICROUDS_DID_ONE_MAX_DATA (MICROUDS_RESPONSE_BUF_SIZE - 3u)

static bool MicroUds_Did_SecurityPass(uint8_t security_level, uint8_t permis) // [REVIEW] 参数名 sercurity -> security
{
    uint8_t need = MICROUDS_PERMIS_LEVEL(permis);

    if (need == 0u)
    {
        return true; // 无需解锁
    }

    if (MICROUDS_GET_ASSIG(permis))
    {
        return security_level == need; // 指定位: 只有恰好这个等级才允许
    }

    return security_level >= need; // 已解锁等级 >= 要求等级
}

/**
 * @brief DID 级访问检查: 会话 + 安全
 * [REVIEW] 原来的检查有三个问题:
 *   1. 会话: `ctx->session & did_item->session`，左边是会话"编号"(如3)，右边是掩码，两个不是同一种东西，要先 MICROUDS_SET_SESSION() 转成位
 *   2. 安全: `if (SecurityPass(...)) { 返回 SAD }` 条件写反了，权限满足时反而被拒绝
 *   3. 会话不满足返回 0x22(CNC)，常见规范是 0x31(DID在当前会话不支持)，改成可配置宏 MICROUDS_DID_NRC_SESSION
 * @return 0 通过，否则为应返回的 NRC
 */
static uint8_t MicroUds_Did_Access(const MicroUds_ServiceCtx_t *ctx, const MicroUdsDidItem_t *item)
{
    if (item->session != 0u && (item->session & MICROUDS_SET_SESSION(ctx->session)) == 0u)
    {
        return (uint8_t)MICROUDS_DID_NRC_SESSION;
    }

    if (!MicroUds_Did_SecurityPass(ctx->sec_level, item->permis))
    {
        return (uint8_t)MICROUDS_NRC_SAD;
    }

    return 0u;
}

MicroUds_Status_t MicroUds_Did_Init(void)
{
    memset(&did_obj, 0, sizeof(MicroUdsDid_Obj_t));

    return MICROUDS_OK;
}

/**
 * @brief 注册读/写表的公共部分
 * [REVIEW] 原来的 RegisterReadItem/RegisterWriteItem 有这些问题:
 *   1. 循环 `i < MICROUDS_DID_READ_SIZE` 而不是 `i < size`: 表只有3项时，会继续读 table[3..7]，越界读
 *   2. 没有检查 size 是否超过容量；没有检查DID重复；没有检查 data/长度是否有效(data==NULL 时 memcpy 直接崩)
 *   3. 第二次注册会把第一张表冲掉(read_num = size)
 *   你的注释是"只能注册一个表，不允许附加"。我改成了"可以多次注册，追加在后面"，原因: 不同的业务模块(应用、标定、刷写)
 *   各自注册自己的DID，比让某一个模块把所有DID集中到一张表里更好维护；如果你就是想限制成一次，把 *num != 0 时直接返回 ERR 即可。
 */
static MicroUds_Status_t MicroUds_Did_RegisterItems(MicroUdsDidItem_t *dst, uint16_t *num, uint16_t max,
                                                    const MicroUdsDidItem_Config_t *table, size_t size, bool is_write)
{
    if (table == NULL || size == 0u || size > max || ((size_t)*num + size) > max)
    {
        return MICROUDS_PARAM_ERR;
    }

    for (size_t i = 0; i < size; i++)
    {
        if (table[i].data_len == 0u)
        {
            return MICROUDS_PARAM_ERR;
        }

        if (is_write)
        {
            // 写: data 和 cb 至少有一个 (cb 负责提交，data 是内核替你拷贝的目的地)；请求缓冲要装得下 SID+DID+数据
            if ((table[i].data == NULL && table[i].cb == NULL) ||
                ((uint32_t)table[i].data_len + 3u) > MICROUDS_REQUEST_BUF_SIZE)
            {
                return MICROUDS_PARAM_ERR;
            }
        }
        else
        {
            // 读: 内核从 data 拷贝数据进响应(cb 只是读之前的"刷新"钩子)，所以 data 必须有
            if (table[i].data == NULL || table[i].data_len > MICROUDS_DID_ONE_MAX_DATA)
            {
                return MICROUDS_PARAM_ERR;
            }
        }

        for (uint16_t k = 0; k < *num; k++) // 与已注册的重复
        {
            if (dst[k].Identifier == table[i].Identifier)
            {
                return MICROUDS_PARAM_ERR;
            }
        }

        for (size_t k = 0; k < i; k++) // 与本次表内的重复
        {
            if (table[k].Identifier == table[i].Identifier)
            {
                return MICROUDS_PARAM_ERR;
            }
        }
    }

    for (size_t i = 0; i < size; i++)
    {
        MicroUdsDidItem_t *item = &dst[*num];
        item->cb = table[i].cb;
        item->data = table[i].data;
        item->data_len = table[i].data_len;
        item->Identifier = table[i].Identifier;
        item->permis = table[i].permis;
        item->session = table[i].session;
        item->Userdata = table[i].Userdata;

        item->is_valid = true;
        (*num)++;
    }

    return MICROUDS_OK;
}

// 只能注册一个表，不允许附加  [REVIEW] 见 MicroUds_Did_RegisterItems 上面的说明，现在可以追加
MicroUds_Status_t MicroUds_Did_RegisterReadItem(const MicroUdsDidItem_Config_t *table, size_t size)
{
    return MicroUds_Did_RegisterItems(did_obj.Read, &did_obj.read_num, MICROUDS_DID_READ_SIZE, table, size, false);
}

MicroUds_Status_t MicroUds_Did_RegisterWriteItem(const MicroUdsDidItem_Config_t *table, size_t size)
{
    return MicroUds_Did_RegisterItems(did_obj.Write, &did_obj.write_num, MICROUDS_DID_WRITE_SIZE, table, size, true);
}

static MicroUdsDidItem_t *MicroUds_Did_FindReadIdentifier(uint16_t Identifier)
{
    for (uint16_t i = 0; i < did_obj.read_num; i++)
    {
        if (Identifier == did_obj.Read[i].Identifier)
        {
            if (!did_obj.Read[i].is_valid)
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
            if (!did_obj.Write[i].is_valid)
            {
                return NULL;
            }
            return &did_obj.Write[i];
        }
    }

    return NULL;
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

/**
 * @brief 读之前的刷新: 调用用户 cb，让用户把最新数据放进 item->data
 * @return 0 成功，否则 NRC
 */
static uint8_t MicroUds_Did_Refresh(const MicroUds_ServiceCtx_t *ctx, const MicroUdsDidItem_t *item, uint16_t did_cnt)
{
    if (item->cb == NULL)
    {
        return 0u;
    }

    MicroUdsDid_Ctx_t didctx = {0};

    didctx.Did = item->Identifier;
    didctx.Did_len = did_cnt;
    didctx.data = item->data;
    didctx.data_len = item->data_len;
    didctx.Userdata = item->Userdata;
    didctx.is_write = false;
    didctx.session = ctx->session;
    didctx.sec_level = ctx->sec_level;

    item->cb(&didctx);

    if (didctx.nrc != 0u)
    {
        return didctx.nrc;
    }

    if (didctx.pending)
    {
        return (uint8_t)MICROUDS_NRC_GR; // 读请求不支持 pending (一条请求里可能有多个DID，没法只挂起其中一个)
    }

    return 0u;
}

/**
 * @brief 动态DID读取前检查: 每个来源都要通过自己的会话/安全检查
 * [REVIEW] 原来读动态DID时完全没有权限检查。动态DID只是"换了个名字读同样的数据"，不能借它绕过源DID的会话/安全限制。
 */
static uint8_t MicroUds_Did_DynaCheck(const MicroUds_ServiceCtx_t *ctx, const MicroUdsDid_Dynamically_t *dyn)
{
    for (uint16_t i = 0; i < dyn->item_num; i++)
    {
        const MicroUdsDid_DynaItem_t *it = &dyn->Item[i];

        if (dyn->type == MICROUDSDID_DYNA_ID)
        {
            const MicroUdsDidItem_t *src = MicroUds_Did_FindReadIdentifier(it->src_did);

            if (src == NULL)
            {
                return (uint8_t)MICROUDS_NRC_ROOR;
            }

            uint8_t nrc = MicroUds_Did_Access(ctx, src);

            if (nrc != 0u)
            {
                return nrc;
            }
        }
        else
        {
#if MICROUDS_DID_DYNA_BY_ADDR
            if (!MicroUds_Did_AddrReadable(it->addr, it->size))
            {
                return (uint8_t)MICROUDS_NRC_ROOR;
            }
#else
            return (uint8_t)MICROUDS_NRC_ROOR;
#endif
        }
    }

    return 0u;
}

/**
 * @brief 把动态DID的数据按顺序读进 out
 * @return 0 成功，否则 NRC
 */
static uint8_t MicroUds_Did_DynaRead(const MicroUds_ServiceCtx_t *ctx, const MicroUdsDid_Dynamically_t *dyn,
                                     uint8_t *out, uint16_t did_cnt)
{
    for (uint16_t i = 0; i < dyn->item_num; i++)
    {
        const MicroUdsDid_DynaItem_t *it = &dyn->Item[i];

        if (dyn->type == MICROUDSDID_DYNA_ID)
        {
            const MicroUdsDidItem_t *src = MicroUds_Did_FindReadIdentifier(it->src_did);

            if (src == NULL)
            {
                return (uint8_t)MICROUDS_NRC_ROOR;
            }

            uint8_t nrc = MicroUds_Did_Refresh(ctx, src, did_cnt); // 先让用户刷新源数据

            if (nrc != 0u)
            {
                return nrc;
            }

            memcpy(out, src->data + (it->pos - 1u), it->size); // 按"起始位置/长度"取片段(位置从1开始)
        }
        else
        {
#if MICROUDS_DID_DYNA_BY_ADDR
            memcpy(out, (const void *)(uintptr_t)it->addr, it->size);
#else
            return (uint8_t)MICROUDS_NRC_ROOR;
#endif
        }

        out += it->size;
    }

    return 0u;
}

// 22服务回调
// [REVIEW] 原来的实现有这些问题(逐条列出，方便你对照):
//   1. DID 下标: `ctx->req[i]` / `ctx->req[i+1]`，第 i 个DID应该是 req[2*i] / req[2*i+1]
//   2. 内层 for 又用了 i，遮蔽了外层变量
//   3. 只要请求里有一个动态DID，就把整条请求当动态处理，其余静态DID被丢掉；而且 `is_dync` 路径完全不做权限检查
//   4. 响应里没有回显 DID (应该是 62 + [DID数据]*N)，payload 偏移用的是 last_len(上一个DID的长度) 而不是累计长度
//   5. 安全检查条件写反；会话检查拿"编号"和"掩码"做与
//   6. 边读边写响应: 第2个DID不存在时，第1个的数据已经读出来(可能有副作用)，却要整条回负响应
//   7. `(uint16_t)(did_size * 0.5f)` 用了浮点，MCU上没必要，直接 /2
//   现在分两阶段: 阶段1 校验全部DID(存在/会话/安全/总长度，不读数据)；阶段2 才读取并拼响应
void MicroUds_Did_0x22ReadHandler(MicroUds_ServiceCtx_t *ctx)
{
    // 请求长度 + 1 就算是did长度了吧
    // [REVIEW] 不需要 +1。0x22 注册成"没有子功能的服务"，ctx->req 从 SID 后面第一个字节开始，ctx->req_len 就是 DID 列表的字节数(2*N)

    uint16_t did_size = (uint16_t)ctx->req_len; // 子功能长度 + 请求数据 [REVIEW] 0x22没有子功能，这里就是纯 DID 列表长度

    if (did_size == 0u || (did_size % 2u) != 0u)
    {
        MicroUds_CtxNegative(ctx, MICROUDS_NRC_IMLOI);
        return; // 返回负响应
    }

    uint16_t did_len = (uint16_t)(did_size / 2u); // / 2

    if (did_len > MICROUDS_DID_READ_MAX_PER_REQ)
    {
        MicroUds_CtxNegative(ctx, MICROUDS_NRC_ROOR); // 一条请求带的DID太多
        return;
    }

    // 先找一下动态did，再去翻静态did
    // [REVIEW] 每个DID独立判断是动态还是静态，不再整条请求二选一
    const MicroUdsDidItem_t *static_sel[MICROUDS_DID_READ_MAX_PER_REQ] = {0};
    const MicroUdsDid_Dynamically_t *dyna_sel[MICROUDS_DID_READ_MAX_PER_REQ] = {0};
    uint16_t dids[MICROUDS_DID_READ_MAX_PER_REQ] = {0};
    uint32_t total_len = 0;
    uint16_t found = 0;

    /* ---------- 阶段1: 校验，不读数据 ---------- */
    for (uint16_t i = 0; i < did_len; i++)
    {
        uint16_t did = (uint16_t)(((uint16_t)ctx->req[2u * i] << 8) | ctx->req[2u * i + 1u]);
        uint8_t nrc = 0u;

        dids[i] = did;

        const MicroUdsDid_Dynamically_t *dyna_item = MicroUds_Did_FindDynamic(did);

        if (dyna_item != NULL)
        {
            // 动态DID
            nrc = MicroUds_Did_DynaCheck(ctx, dyna_item);
            if (nrc != 0u)
            {
                MicroUds_CtxNegative(ctx, nrc);
                return;
            }
            dyna_sel[i] = dyna_item;
            total_len += 2u + dyna_item->data_len;
            found++;
            continue;
        }

        const MicroUdsDidItem_t *did_item = MicroUds_Did_FindReadIdentifier(did);

        if (did_item == NULL)
        {
#if MICROUDS_DID_READ_PARTIAL
            continue; // 宽松策略: 跳过不存在的DID
#else
            MicroUds_CtxNegative(ctx, MICROUDS_NRC_ROOR);
            return;
#endif
        }

        nrc = MicroUds_Did_Access(ctx, did_item);
        if (nrc != 0u)
        {
#if MICROUDS_DID_READ_PARTIAL
            if (nrc == MICROUDS_DID_NRC_SESSION)
            {
                continue; // 当前会话不支持这个DID，跳过 (安全不满足仍然整条拒绝)
            }
#endif
            MicroUds_CtxNegative(ctx, nrc);
            return;
        }

        static_sel[i] = did_item;
        total_len += 2u + did_item->data_len;
        found++;
    }

    if (found == 0u)
    {
        MicroUds_CtxNegative(ctx, MICROUDS_NRC_ROOR); // 全部不支持
        return;
    }

    if (total_len > ctx->payload_cap) // [REVIEW] 原来是 "payload_cap < 单个DID长度"，多个DID累计超限检查不到
    {
        MicroUds_CtxNegative(ctx, MICROUDS_NRC_RTL);
        return;
    }

    /* ---------- 阶段2: 读取并拼响应 ---------- */
    uint8_t *p = ctx->payload;

    for (uint16_t i = 0; i < did_len; i++)
    {
        uint8_t nrc = 0u;

        if (static_sel[i] != NULL)
        {
            const MicroUdsDidItem_t *did_item = static_sel[i];

            nrc = MicroUds_Did_Refresh(ctx, did_item, did_len);
            if (nrc != 0u)
            {
                MicroUds_CtxNegative(ctx, nrc);
                return; // 可能是pedding啥的吧 [REVIEW] 读不支持pending，见 MicroUds_Did_Refresh
            }

            p[0] = (uint8_t)(did_item->Identifier >> 8); // 响应里要回显DID
            p[1] = (uint8_t)(did_item->Identifier & 0xFFu);
            memcpy(&p[2], did_item->data, did_item->data_len);
            p += 2u + did_item->data_len;
        }
        else if (dyna_sel[i] != NULL)
        {
            const MicroUdsDid_Dynamically_t *dyna_item = dyna_sel[i];

            p[0] = (uint8_t)(dids[i] >> 8);
            p[1] = (uint8_t)(dids[i] & 0xFFu);

            nrc = MicroUds_Did_DynaRead(ctx, dyna_item, &p[2], did_len);
            if (nrc != 0u)
            {
                MicroUds_CtxNegative(ctx, nrc);
                return;
            }
            p += 2u + dyna_item->data_len;
        }
    }

    MicroUds_CtxPositive(ctx, (uint32_t)(p - ctx->payload));
}

// [REVIEW] 原来的 2E 实现有这些问题:
//   1. `ctx->req_len - 3` 是无符号减法，请求只有 DID 没有数据时会下溢成很大的数；而且0x2E没有子功能，数据长度应该是 req_len - 2
//   2. `did_len % 2` 判断没有意义: 数据长度是奇数偶数都合法，该检查的是"等于DID定义的长度"
//   3. `memcpy(Item->data, ctx->req, ctx->req_len)`: 来源是整个请求(含2字节DID)，长度没限制，会越界写用户变量；
//      而且是在校验之前就写进去了，校验不通过变量已经被改坏
//   4. 没有做会话/安全检查
//   5. `Item->cb` 没判空
//   6. 正响应应该回显DID(6E DIDH DIDL)，原来只给了长度，payload没写
void MicroUds_Did_0x2EWriteHandler(MicroUds_ServiceCtx_t *ctx)
{
    if (ctx->req_len < 3u) // DID(2) + 至少1字节数据
    {
        MicroUds_CtxNegative(ctx, MICROUDS_NRC_IMLOI);
        return;
    }

    uint16_t did = (uint16_t)(((uint16_t)ctx->req[0] << 8) | ctx->req[1]); // 组合did
    const uint8_t *in = &ctx->req[2];
    uint16_t in_len = (uint16_t)(ctx->req_len - 2u);

    MicroUdsDidItem_t *Item = MicroUds_Did_FindWriteIdentifier(did);

    // 动态DID不在写表里，所以自然查不到 -> 0x31，满足"动态DID不能用 0x2E 写"
    if (Item == NULL)
    {
        MicroUds_CtxNegative(ctx, MICROUDS_NRC_ROOR);
        return;
    }

    // 校验只在首次调用时做; pending 之后的重入不再重复校验(权限/长度没变，只是继续等用户cb完成)
    if (ctx->call_cnt == 0u)
    {
        uint8_t nrc = MicroUds_Did_Access(ctx, Item);

        if (nrc != 0u)
        {
            MicroUds_CtxNegative(ctx, nrc);
            return;
        }

        // if(did_len > Item->data_len)
        // {
        // 这里应该是负响应吗
        // }
        // [REVIEW] 是，用 0x13。固定长度的DID，写入长度必须严格等于定义长度(多了少了都是 0x13)
        if (in_len != Item->data_len)
        {
            MicroUds_CtxNegative(ctx, MICROUDS_NRC_IMLOI);
            return;
        }
    }

    // 校验通过，先让用户的cb做"数据层校验+提交"，接受(nrc==0)之后内核才替用户把数据拷进 data
    if (Item->cb != NULL)
    {
        MicroUdsDid_Ctx_t didctx = {0};
        didctx.Did = Item->Identifier;
        didctx.Did_len = 1;
        didctx.Userdata = Item->Userdata;
        didctx.data = Item->data;
        didctx.data_len = Item->data_len;
        didctx.is_write = true;
        didctx.in_data = in;
        didctx.in_len = in_len;
        didctx.session = ctx->session;
        didctx.sec_level = ctx->sec_level;
        didctx.call_cnt = ctx->call_cnt;
        Item->cb(&didctx);

        if (didctx.nrc != 0)
        {
            MicroUds_CtxNegative(ctx, didctx.nrc);
            return;
        }

        if (didctx.pending)
        {
            MicroUds_CtxPending(ctx); // 还没写完: 内核回 0x78，下个周期再次进来(call_cnt>0)
            return;
        }
    }

    if (Item->data != NULL)
    {
        memcpy(Item->data, in, in_len); // 执行写入
    }

    ctx->payload[0] = (uint8_t)(did >> 8); // 6E DIDH DIDL
    ctx->payload[1] = (uint8_t)(did & 0xFFu);
    MicroUds_CtxPositive(ctx, 2u);
}

/* ======================================================================
 *                          0x2C 动态定义 DID
 * ====================================================================== */

/** 动态DID号是否合法: 在动态区间内，并且没有和静态DID重名 */
static bool MicroUds_Did_DynaIdValid(uint16_t did)
{
    if (did < MICROUDS_DID_DYNA_ID_MIN || did > MICROUDS_DID_DYNA_ID_MAX)
    {
        return false;
    }

    return (MicroUds_Did_FindReadIdentifier(did) == NULL) && (MicroUds_Did_FindWriteIdentifier(did) == NULL);
}

/** 取一个槽位: 已存在就复用(覆盖重新定义)，否则找空闲槽 */
static MicroUdsDid_Dynamically_t *MicroUds_Did_DynaAlloc(uint16_t did)
{
    MicroUdsDid_Dynamically_t *slot = MicroUds_Did_FindDynamic(did);

    if (slot != NULL)
    {
        return slot; // 已经存在了，直接覆盖重新定义 (注意: 不同版本/OEM 对"重复定义同一个DDDID"是覆盖还是追加规定不同，这里按覆盖)
    }

    // [REVIEW] 原来用 `Dynamically[dync_num]` 当空闲位置: 清除中间一个后 dync_num 和空闲位置不再对应，会覆盖正在使用的槽位
    for (uint16_t i = 0; i < MICROUDS_DID_DYNAMICALLY_SIZE; i++)
    {
        if (!did_obj.Dynamically[i].is_active)
        {
            did_obj.dync_num++;
            return &did_obj.Dynamically[i];
        }
    }

    return NULL;
}

static void MicroUds_Did_DynaClear(MicroUdsDid_Dynamically_t *slot)
{
    if (slot->is_active && did_obj.dync_num > 0u)
    {
        did_obj.dync_num--;
    }
    memset(slot, 0, sizeof(MicroUdsDid_Dynamically_t));
}

void MicroUds_Did_OnSessionChange(uint8_t new_ssid)
{
#if MICROUDS_DID_DYNA_CLEAR_ON_DEFAULT
    if (new_ssid == MICROUDS_SESSION_DEFAULT)
    {
        for (uint16_t i = 0; i < MICROUDS_DID_DYNAMICALLY_SIZE; i++)
        {
            MicroUds_Did_DynaClear(&did_obj.Dynamically[i]);
        }
    }
#else
    (void)new_ssid;
#endif
}

// DynamicallyDefineDataIdentifier 01 02 03 内核注册为没有子功能，内核不去解析，我们自己解析
// [REVIEW] 改成了"有子功能字节(HAS_SUBFUNC)、带抑制位"的内置服务，由内核解析子功能:
//   原来注册成"没有子功能"后，ctx->req 从子功能字节开始，所以 `ctx->req[1]` 取到的是DDDID高字节而不是子功能，
//   `switch (ctx->sub_func)` 取的又是内核没填的0，结果是任何0x2C请求都不会走到任何case，也不会有响应。
//   让内核解析子功能还有三个好处: 抑制位(2C 81 ...)自动处理、正响应自动回显子功能(6C 01 ...)、会话/长度检查走统一流水线。
//   现在 ctx->sub_func = 01/02/03，ctx->req 从 DDDID 开始。
void MicroUds_Did_0x2CDynamicHandler(MicroUds_ServiceCtx_t *ctx)
{
    switch (ctx->sub_func)
    {
    // [ask] 我的理解没问题吧 请求帧: [2c 01] [F2 00](这是动态标识符) [10 01](源DID) [01](起始位置) [02](长度) * N
    // [REVIEW] 帧格式理解正确: 2C 01 DDDID(2) { 源DID(2) 起始位置(1) 长度(1) } * N，每个片段4字节。起始位置从1开始(1=源数据的第1字节)。
    //          正响应是 6C 01 DDDID。提醒一点: 标准里 0xF200~0xF2FF 是周期DID(0x2A用)，动态DID在 0xF300~0xF3FF，
    //          你的例子 F2 00 落在周期区间；如果你们OEM约定动态DID用 F2xx，改 conf 里的 MICROUDS_DID_DYNA_ID_MIN/MAX。
    case 0x01: // 定义动态did
    {
        if (ctx->req_len < 6u || ((ctx->req_len - 2u) % 4u) != 0u) // DDDID(2) + 至少一个片段(4)
        {
            MicroUds_CtxNegative(ctx, MICROUDS_NRC_IMLOI);
            return;
        }

        uint16_t dync_did = (uint16_t)(((uint16_t)ctx->req[0] << 8) | ctx->req[1]);
        uint16_t did_num = (uint16_t)((ctx->req_len - 2u) / 4u);

        if (!MicroUds_Did_DynaIdValid(dync_did) || did_num > MICROUDS_DID_DYNAMICALLY_ITEM_LEN)
        {
            MicroUds_CtxNegative(ctx, MICROUDS_NRC_ROOR); // 不在动态区间/和静态DID冲突/片段太多
            return;
        }

        // [REVIEW] 先全部校验、结果放进临时变量，通过后才提交到槽位。原来是边解析边写槽位，中途失败槽位里留着半个定义
        MicroUdsDid_DynaItem_t tmp[MICROUDS_DID_DYNAMICALLY_ITEM_LEN];
        uint32_t total = 0;

        for (uint16_t i = 0; i < did_num; i++)
        {
            // [REVIEW] 原来 `req[i + 3]` 每个片段的下标只前进1，应该每个片段前进4字节
            const uint8_t *seg = &ctx->req[2u + 4u * i];
            uint16_t src_did = (uint16_t)(((uint16_t)seg[0] << 8) | seg[1]);
            uint16_t pos = seg[2];
            uint16_t size = seg[3];

            const MicroUdsDidItem_t *static_item = MicroUds_Did_FindReadIdentifier(src_did);

            if (static_item == NULL)
            {
                MicroUds_CtxNegative(ctx, MICROUDS_NRC_ROOR); // 源DID不存在
                return;
            }

            uint8_t nrc = MicroUds_Did_Access(ctx, static_item); // 定义时就确认你有权读这个源DID
            if (nrc != 0u)
            {
                MicroUds_CtxNegative(ctx, nrc);
                return;
            }

            // 位置从1开始，片段不能超出源数据记录
            if (pos < 1u || size < 1u || (uint32_t)(pos - 1u) + size > static_item->data_len)
            {
                MicroUds_CtxNegative(ctx, MICROUDS_NRC_ROOR);
                return;
            }

            tmp[i].src_did = src_did;
            tmp[i].pos = pos;
            tmp[i].size = size;
            tmp[i].addr = 0;
            total += size;
        }

        if (total > MICROUDS_DID_ONE_MAX_DATA)
        {
            MicroUds_CtxNegative(ctx, MICROUDS_NRC_ROOR); // 读取时响应装不下
            return;
        }

        MicroUdsDid_Dynamically_t *dyna_itme = MicroUds_Did_DynaAlloc(dync_did);

        if (dyna_itme == NULL)
        {
            MicroUds_CtxNegative(ctx, MICROUDS_NRC_ROOR); // 动态DID槽位满了
            return;
        }

        memset(dyna_itme, 0, sizeof(MicroUdsDid_Dynamically_t));
        memcpy(dyna_itme->Item, tmp, sizeof(MicroUdsDid_DynaItem_t) * did_num);
        dyna_itme->dync_did = dync_did;
        dyna_itme->type = MICROUDSDID_DYNA_ID;
        dyna_itme->item_num = did_num;
        dyna_itme->data_len = (uint16_t)total;
        dyna_itme->is_active = true;

        // [REVIEW] 原来是 memcpy(ctx->payload, (uint8_t *)dync_did, 2): 把DID的"数值"当成地址去读，必然崩溃。
        //          正确做法是把DID的两个字节写进响应。子功能(01)由内核自动回显，所以响应是 6C 01 DDDID
        ctx->payload[0] = (uint8_t)(dync_did >> 8);
        ctx->payload[1] = (uint8_t)(dync_did & 0xFFu);
        MicroUds_CtxPositive(ctx, 2); // 返回正响应
    }
    break;

    case 0x02: // 按照地址定义
    {
#if MICROUDS_DID_DYNA_BY_ADDR
        // [2c 02] [F2 00] [24](ALFID 表示地址4字节，长度两字节) [20 00 10 00](内存地址) [00 02](长度) *N

        // 对于ARM 32位单片机，不是4字节直接返回错误就好了吧?
        // [REVIEW] 是的。ALFID 高4位 = 长度字段的字节数，低4位 = 地址字段的字节数。32位MCU只接受地址4字节、长度1~2字节，其余回 0x31。
        //          原来的 `(req[3] & 0x0F) + req[3] >> 4` 有运算符优先级问题: `+` 比 `>>` 优先级高，
        //          实际算的是 ((req[3]&0x0F) + req[3]) >> 4。而且 ALFID 在这个布局里是 req[2](DDDID 之后)，不是 req[3]。
        if (ctx->req_len < 3u)
        {
            MicroUds_CtxNegative(ctx, MICROUDS_NRC_IMLOI);
            return;
        }

        uint16_t dync_did = (uint16_t)(((uint16_t)ctx->req[0] << 8) | ctx->req[1]);
        uint8_t alfid = ctx->req[2];
        uint8_t addr_bytes = (uint8_t)(alfid & 0x0Fu);
        uint8_t size_bytes = (uint8_t)(alfid >> 4);

        if (!MicroUds_Did_DynaIdValid(dync_did) || addr_bytes != 4u || size_bytes < 1u || size_bytes > 2u)
        {
            MicroUds_CtxNegative(ctx, MICROUDS_NRC_ROOR);
            return;
        }

        uint32_t stride = (uint32_t)addr_bytes + size_bytes;
        uint32_t body = ctx->req_len - 3u;

        if (body == 0u || (body % stride) != 0u)
        {
            MicroUds_CtxNegative(ctx, MICROUDS_NRC_IMLOI);
            return;
        }

        uint32_t addr_num = body / stride; // 总字节长度 / 单个长度

        if (addr_num > MICROUDS_DID_DYNAMICALLY_ITEM_LEN)
        {
            MicroUds_CtxNegative(ctx, MICROUDS_NRC_ROOR);
            return;
        }

        MicroUdsDid_DynaItem_t tmp[MICROUDS_DID_DYNAMICALLY_ITEM_LEN];
        uint32_t total = 0;

        for (uint32_t i = 0; i < addr_num; i++)
        {
            const uint8_t *seg = &ctx->req[3u + stride * i];
            uint32_t addr = ((uint32_t)seg[0] << 24) | ((uint32_t)seg[1] << 16) | ((uint32_t)seg[2] << 8) | seg[3];
            uint32_t len = seg[4];

            if (size_bytes == 2u)
            {
                len = (len << 8) | seg[5];
            }

            // [REVIEW] 原来只检查 addr==0。诊断仪可以读任意地址等于把整个内存(包括密钥)暴露出去。
            //          必须由用户实现白名单 MicroUds_Did_AddrReadable()，并检查 addr+len 溢出
            if (len == 0u || (addr + len) < addr || !MicroUds_Did_AddrReadable(addr, len))
            {
                MicroUds_CtxNegative(ctx, MICROUDS_NRC_ROOR);
                return;
            }

            tmp[i].src_did = 0;
            tmp[i].pos = 0;
            tmp[i].size = (uint16_t)len;
            tmp[i].addr = addr;
            total += len;
        }

        if (total > MICROUDS_DID_ONE_MAX_DATA)
        {
            MicroUds_CtxNegative(ctx, MICROUDS_NRC_ROOR);
            return;
        }

        MicroUdsDid_Dynamically_t *dyna_itme = MicroUds_Did_DynaAlloc(dync_did);

        if (dyna_itme == NULL)
        {
            MicroUds_CtxNegative(ctx, MICROUDS_NRC_ROOR);
            return;
        }

        memset(dyna_itme, 0, sizeof(MicroUdsDid_Dynamically_t));
        memcpy(dyna_itme->Item, tmp, sizeof(MicroUdsDid_DynaItem_t) * addr_num);
        dyna_itme->dync_did = dync_did;
        dyna_itme->type = MICROUDSDID_DYNA_ADDRESS;
        dyna_itme->item_num = (uint16_t)addr_num;
        dyna_itme->data_len = (uint16_t)total;
        dyna_itme->is_active = true;

        ctx->payload[0] = (uint8_t)(dync_did >> 8);
        ctx->payload[1] = (uint8_t)(dync_did & 0xFFu);
        MicroUds_CtxPositive(ctx, 2); // 返回正响应
        // dyna_itme.
#else
        // 量产默认关闭按地址定义，子功能不支持
        MicroUds_CtxNegative(ctx, MICROUDS_NRC_SFNS);
#endif
    }
    break;

    case 0x03: // 清除动态
    {
        // [REVIEW] 原来无条件清除全部，而且没有响应。2C 03 [DDDID]: 带DDDID只清这一个，不带则清除全部(不同版本表述不同，请对照你们的规范)
        if (ctx->req_len == 0u)
        {
            for (uint16_t i = 0; i < MICROUDS_DID_DYNAMICALLY_SIZE; i++)
            {
                MicroUds_Did_DynaClear(&did_obj.Dynamically[i]);
            }
            MicroUds_CtxPositive(ctx, 0u);
        }
        else if (ctx->req_len == 2u)
        {
            uint16_t dync_did = (uint16_t)(((uint16_t)ctx->req[0] << 8) | ctx->req[1]);
            MicroUdsDid_Dynamically_t *slot = MicroUds_Did_FindDynamic(dync_did);

            if (slot == NULL)
            {
                MicroUds_CtxNegative(ctx, MICROUDS_NRC_ROOR);
                return;
            }

            MicroUds_Did_DynaClear(slot);
            ctx->payload[0] = (uint8_t)(dync_did >> 8);
            ctx->payload[1] = (uint8_t)(dync_did & 0xFFu);
            MicroUds_CtxPositive(ctx, 2u);
        }
        else
        {
            MicroUds_CtxNegative(ctx, MICROUDS_NRC_IMLOI);
        }
    }
    break;

    default:
        MicroUds_CtxNegative(ctx, MICROUDS_NRC_SFNS); // 子功能不支持
        break;
    }
}

/**
 * @brief 弱函数，用户重写: 默认全部拒绝
 */
bool __attribute__((weak)) MicroUds_Did_AddrReadable(uint32_t addr, uint32_t len)
{
    (void)addr;
    (void)len;
    return false;
}

#endif