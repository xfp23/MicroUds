/**
 * @file MicroUds.c
 * @author xfp23
 * @brief MicroUds实现
 * @version 0.2
 * @date 2026-10-21
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "MicroUds.h"

// MICROUDS_INSTANCE_NUM 先不做多实例

// [REVIEW] 原来是非static的全局，没有任何地方在外部引用它，改为文件内可见
static MicroUds_Obj_t uds_obj = {0};

static bool MicroUds_SecurityVerify(void);
static void MicroUds_SecurityLock(void);
static void MicroUds_SessionSwitch(uint8_t ssid);
static void MicroUds_RequestDone(void);
static void MicroUds_ServiceRun(const MicroUds_ServiceEntry_t *serv, const MicroUds_SubFunc_t *sub);

/* ======================================================================
 *                              响应构造
 * ====================================================================== */

/**
 * @brief 构造正响应  RSID(+子功能) + payload
 * [REVIEW] 1. 增加了长度检查(原来 memcpy 没有检查，响应缓冲32字节，payload 超长直接踩内存)
 *          2. 有子功能的服务，正响应必须回显子功能(如 50 03 ..、67 01 <seed>、7E 00)，原来没有回显
 *          3. 请求抑制位、寻址方式在这里就保存进响应，不再等发送时去读(那时 pdu 可能已被新请求覆盖)
 */
static MicroUds_Status_t NegativeResponse(uint8_t sid, uint8_t nrc);

static MicroUds_Status_t PositiveResponse(uint8_t sid, bool has_sub, uint8_t sub, const uint8_t *payload, uint32_t len)
{
    if (uds_obj.resp.active)
    {
        return MICROUDS_BUSY;
    }

    uint32_t off = has_sub ? 2u : 1u;

    if (payload == NULL)
    {
        len = 0;
    }

    if (off + len > MICROUDS_RESPONSE_BUF_SIZE)
    {
        return NegativeResponse(sid, MICROUDS_NRC_RTL); // 响应过长
    }

    uds_obj.resp.type = MICROUDS_RESPTYPE_POSITIVE; // 正响应
    uds_obj.resp.addr = uds_obj.pdu.addr;
    uds_obj.resp.Inhibit = uds_obj.pdu.suppress ? 1u : 0u;
    uds_obj.resp.apply_ssid = 0;

    uds_obj.resp.data[0] = (uint8_t)(sid + 0x40u);
    if (has_sub)
    {
        uds_obj.resp.data[1] = sub;
    }

    // payload 通常已经就在响应缓冲里(回调直接写进去的)，这时不需要再拷贝
    if (len > 0u && payload != &uds_obj.resp.data[off])
    {
        memmove(&uds_obj.resp.data[off], payload, len);
    }

    uds_obj.resp.len = off + len;
    uds_obj.resp.active = true;

    return MICROUDS_OK;
}

/**
 * @brief 负响应
 *
 * @param nrc
 * [REVIEW] 负响应不受"抑制正响应"位影响，Inhibit 恒为0。原来是只要请求带了 0x80 位，负响应也被吞掉
 */
static MicroUds_Status_t NegativeResponse(uint8_t sid, uint8_t nrc)
{
    if (uds_obj.resp.active)
    {
        return MICROUDS_BUSY;
    }

    uds_obj.resp.type = MICROUDS_RESPTYPE_NEGATIVE; // 负响应
    uds_obj.resp.addr = uds_obj.pdu.addr;
    uds_obj.resp.Inhibit = 0;
    uds_obj.resp.apply_ssid = 0;

    uds_obj.resp.data[0] = 0x7F;
    uds_obj.resp.data[1] = sid;
    uds_obj.resp.data[2] = nrc;

    uds_obj.resp.len = 3;
    uds_obj.resp.active = true;

    return MICROUDS_OK;
}

/**
 * @brief 功能寻址下需要静默的负响应
 * [REVIEW] 功能寻址时 0x11/0x12/0x31/0x7E/0x7F 的负响应不发送(广播请求里"不是我的服务"不应该所有ECU都回一遍)。
 *          功能寻址的"正响应"是正常发送的。具体以你们OEM规范为准。
 */
static bool MicroUds_IsSilentNrc(uint8_t nrc)
{
    return (nrc == MICROUDS_NRC_SNS) || (nrc == MICROUDS_NRC_SFNS) || (nrc == MICROUDS_NRC_ROOR) ||
           (nrc == MICROUDS_NRC_SFNSIAS) || (nrc == MICROUDS_NRC_SNSIAS);
}

static void MicroUds_ResponseHandler(void)
{
    if (!uds_obj.resp.active)
    {
        return;
    }

    // [REVIEW] 原来是 "!Inhibit && addr == PHY 才发"，导致功能寻址的正响应永远不发、被抑制时负响应也被吞。
    bool drop = false;

    if (uds_obj.resp.type == MICROUDS_RESPTYPE_POSITIVE)
    {
        drop = (uds_obj.resp.Inhibit != 0u);
    }
    else
    {
        drop = (uds_obj.resp.addr == MICROUDS_ADDR_FUNC) && MicroUds_IsSilentNrc(uds_obj.resp.data[2]);
    }

    if (!drop)
    {
        MicroUds_Status_t st = MicroUds_Transmit((const uint8_t *)uds_obj.resp.data, uds_obj.resp.len);

        if (st == MICROUDS_BUSY)
        {
            return; // [REVIEW] 传输层忙: 保留响应，下个周期重试(原来不管返回值，直接清掉，响应就丢了)
        }
        // 其他错误: 丢弃这条响应。后续可以加重试次数/错误回调
    }

    // 响应已处理(发出或按规则静默)，此时才让 0x10 的会话切换生效
    if (uds_obj.resp.apply_ssid != 0u)
    {
        MicroUds_SessionSwitch(uds_obj.resp.apply_ssid);
    }

    memset(&uds_obj.resp, 0, sizeof(MicroUds_Response_t));
}

/* ======================================================================
 *                              会话 / 安全 状态
 * ====================================================================== */

/**
 * @brief 锁定安全访问 (回到未解锁状态)
 * [REVIEW] 只清"解锁状态"和"进行中的种子"，不清 attempts/delay。
 *          原来S3超时时 memset 整个 security，失败计数一起清零，诊断仪可以靠等S3超时来无限重试。
 */
static void MicroUds_SecurityLock(void)
{
    uds_obj.security.permis = 0;
    uds_obj.security.level = 0;
    uds_obj.security.step = MICROUDS_SEC_STEP_IDLE;
    uds_obj.security.seed_len = 0;
    memset(uds_obj.security.seed, 0, sizeof(uds_obj.security.seed));
    memset(uds_obj.security.key, 0, sizeof(uds_obj.security.key));
    memset(uds_obj.security.key_len, 0, sizeof(uds_obj.security.key_len));
}

/**
 * @brief 会话切换的唯一入口
 * [REVIEW] 原来 0x10 切会话和 S3 超时回默认会话是两处各写一份，而且 0x10 路径没更新 is_default、没锁安全。
 *          会话变化时统一: 更新会话、重新锁定安全访问、重启S3。
 *          (切到"同一个会话"是否要重新锁定，各OEM规范不同，这里按"会话没变就不锁"处理)
 */
static void MicroUds_SessionSwitch(uint8_t ssid)
{
    if (ssid != uds_obj.session.ssid)
    {
        MicroUds_SecurityLock();
    }

    uds_obj.session.ssid = ssid;
    uds_obj.session.is_default = (ssid == MICROUDS_SESSION_DEFAULT);
    uds_obj.session.last_tick = uds_obj.tick;
}

static bool MicroUds_SecurityVerify(void)
{
    if (uds_obj.security.key_len[0] != MICROUDS_KEY_SIZE || uds_obj.security.key_len[1] != MICROUDS_KEY_SIZE)
    {
        return false;
    }

    // [REVIEW] 原来是 key[0][i] != key[1][0] (第二个下标写死成0，只会比较第一个字节)。
    //          这里用"异或累加后一次判断"，比较时间与密钥内容无关，顺便避免时序侧信道
    uint8_t diff = 0;

    for (uint32_t i = 0; i < MICROUDS_KEY_SIZE; i++)
    {
        diff |= (uint8_t)(uds_obj.security.key[0][i] ^ uds_obj.security.key[1][i]);
    }
    return diff == 0u;
}

/**
 * @brief 服务/子功能的安全等级检查
 * [REVIEW] 原来 dedic=1 时 "permis(含0x80位) != 当前等级" 永远成立(0x82 != 2)，所有指定位服务都被拒；
 *          dedic=0(>=规则)的分支根本没有实现，等于完全不检查权限。这里按注释的规则实现。
 */
static bool MicroUds_SecurityPass(uint8_t permis)
{
    uint8_t need = MICROUDS_PERMIS_LEVEL(permis);

    if (need == 0u)
    {
        return true; // 无需解锁
    }

    if (MICROUDS_GET_ASSIG(permis))
    {
        return uds_obj.security.permis == need; // 指定位: 只有恰好这个等级才允许
    }

    return uds_obj.security.permis >= need; // 已解锁等级 >= 要求等级
}

/* ======================================================================
 *                         内核内置服务 0x10 / 0x27 / 0x3E
 *   [REVIEW] 回答 "诊断会话控制作为内核基本服务直接实现？应该是正确的吧":
 *   是正确的。但不要写在 RxIndication 里(那是传输层上下文，可能在中断里)，
 *   而是作为"内置服务"注册进同一张服务表，走和用户服务完全相同的流水线
 *   (会话检查、长度检查、抑制位、安全检查、响应构造)，不会出现内置服务绕过校验的情况。
 * ====================================================================== */

// 0x10 DiagnosticSessionControl
static void MicroUds_SvcSessionControl(MicroUds_ServiceCtx_t *ctx)
{
    uint8_t target = ctx->sub_func;

    // [REVIEW] 不支持的会话应返回 0x12 SubFunctionNotSupported，而不是 0x10 GeneralReject。
    //          target 先判断范围再移位，原来 1 << data[1] 在 data[1]>=32(或带抑制位0x80)时是未定义行为
    if (target == 0u || target >= 32u || (uds_obj.session.mask & MICROUDS_SET_SESSION(target)) == 0u)
    {
        MicroUds_CtxNegative(ctx, MICROUDS_NRC_SFNS);
        return;
    }

    // [REVIEW] 0x10 的正响应: 50 <会话> <P2 高 低> <P2* 高 低>(P2*单位10ms)，原来只回了一个 0x50
    uint16_t p2 = (uint16_t)MICROUDS_P2_SERVER_MS;
    uint16_t p2s = (uint16_t)(MICROUDS_P2STAR_SERVER_MS / 10u);

    ctx->payload[0] = (uint8_t)(p2 >> 8);
    ctx->payload[1] = (uint8_t)(p2 & 0xFFu);
    ctx->payload[2] = (uint8_t)(p2s >> 8);
    ctx->payload[3] = (uint8_t)(p2s & 0xFFu);

    uds_obj.session.pending_ssid = target; // 正响应发出之后才真正切换
    MicroUds_CtxPositive(ctx, 4u);
}

// 0x3E TesterPresent
static void MicroUds_SvcTesterPresent(MicroUds_ServiceCtx_t *ctx)
{
    // S3 在收到请求时已经刷新，这里只需要回应。 3E 80 的抑制由流水线处理
    if (ctx->sub_func != 0x00u)
    {
        MicroUds_CtxNegative(ctx, MICROUDS_NRC_SFNS);
        return;
    }
    MicroUds_CtxPositive(ctx, 0u);
}

// 0x27 SecurityAccess
// [REVIEW] 原来的 SecurityHandler 有一串问题:
//   - 请求种子时只算了 "期望密钥"，却从没有把种子发回给诊断仪(没有 PositiveResponse)
//   - seed_len >= SEED_SIZE 判错，正好等于 SEED_SIZE 的合法种子被当成错误
//   - 密钥校验失败后 step 停在3、active 保持true，每个周期都重复发一次 NRC 0x35
//   - sendKey 时 memcpy(&data[1]) 拷到的是子功能字节而不是密钥(应从 data[2] 开始)，且没有长度检查
//   - 不管 27 的子功能是奇数(请求种子)还是偶数(发送密钥)，都当"请求种子"处理
//   - GetKey 没有种子入参，用户无法算出密钥
//   现在改为在服务分发里同步处理，一次请求一次响应，不再需要 step=1/2/3 的中间态
static void MicroUds_SvcSecurityAccess(MicroUds_ServiceCtx_t *ctx)
{
    MicroUds_Security_t *sec = &uds_obj.security;
    uint8_t sub = ctx->sub_func;

    if (sub == 0u || sub > (uint8_t)(2u * MICROUDS_SECURITY_MAX_LEVEL))
    {
        MicroUds_CtxNegative(ctx, MICROUDS_NRC_SFNS);
        return;
    }

    uint8_t level = (uint8_t)((sub + 1u) / 2u); // 27 01/02 -> 等级1，27 03/04 -> 等级2 ...

    if ((sub & 1u) != 0u)
    {
        /* ---------- 奇数子功能: requestSeed ---------- */
        if (sec->delay_active)
        {
            if (!MICROUDS_TICK_ELAPSED(uds_obj.tick, sec->delay_start, MICROUDS_MS_TO_TICK(MICROUDS_SECURITY_DELAY_MS)))
            {
                MicroUds_CtxNegative(ctx, MICROUDS_NRC_RTDSNE); // 延时还没到
                return;
            }
            sec->delay_active = false; // 延时结束，重新给机会
            sec->attempts = 0;
        }

        if (ctx->payload_cap < MICROUDS_SEED_SIZE)
        {
            MicroUds_CtxNegative(ctx, MICROUDS_NRC_RTL);
            return;
        }

        // 该等级已经解锁: 按标准返回全0种子
        if (sec->permis == level)
        {
            memset(ctx->payload, 0, MICROUDS_SEED_SIZE);
            MicroUds_CtxPositive(ctx, MICROUDS_SEED_SIZE);
            return;
        }

        // 得到种子并返回
        uint32_t seed_len = MICROUDS_SEED_SIZE; // [in] 容量 [out] 实际长度
        memset(sec->seed, 0, sizeof(sec->seed));
        MicroUds_SecurityReqSeed(level, sec->seed, &seed_len);

        if (seed_len == 0u || seed_len > MICROUDS_SEED_SIZE)
        {
            sec->step = MICROUDS_SEC_STEP_IDLE;
            MicroUds_CtxNegative(ctx, MICROUDS_NRC_GR); // 用户没有实现种子生成，或长度非法
            return;
        }

        sec->seed_len = seed_len;
        sec->level = level;
        sec->step = MICROUDS_SEC_STEP_SEED_SENT;

        memcpy(ctx->payload, sec->seed, seed_len);
        MicroUds_CtxPositive(ctx, seed_len);
        return;
    }

    /* ---------- 偶数子功能: sendKey ---------- */
    // 必须紧跟同等级的 requestSeed，否则 0x24
    if (sec->step != MICROUDS_SEC_STEP_SEED_SENT || sec->level != level)
    {
        MicroUds_CtxNegative(ctx, MICROUDS_NRC_RSE);
        return;
    }

    if (ctx->req_len != MICROUDS_KEY_SIZE)
    {
        MicroUds_CtxNegative(ctx, MICROUDS_NRC_IMLOI); // 密钥长度错误，种子保留，诊断仪可以重发
        return;
    }

    memcpy(sec->key[1], ctx->req, MICROUDS_KEY_SIZE); // 诊断仪发来的密钥
    sec->key_len[1] = MICROUDS_KEY_SIZE;

    sec->key_len[0] = MICROUDS_KEY_SIZE; // [in] 容量 [out] 实际长度
    memset(sec->key[0], 0, MICROUDS_KEY_SIZE);
    MicroUds_SecurityGetKey(level, sec->seed, sec->seed_len, sec->key[0], &sec->key_len[0]);

    bool ok = MicroUds_SecurityVerify();

    // 一次 sendKey 之后种子立即作废，无论成功与否
    sec->step = MICROUDS_SEC_STEP_IDLE;
    memset(sec->seed, 0, sizeof(sec->seed));
    sec->seed_len = 0;
    memset(sec->key, 0, sizeof(sec->key));

    if (ok)
    {
        sec->permis = level;
        sec->attempts = 0;
        MicroUds_CtxPositive(ctx, 0u);
        return;
    }

    sec->attempts++;
    if (sec->attempts >= MICROUDS_SECURITY_MAX_ATTEMPTS)
    {
        sec->delay_active = true;
        sec->delay_start = uds_obj.tick;
        MicroUds_CtxNegative(ctx, MICROUDS_NRC_ENOA); // 超过尝试次数
        return;
    }
    MicroUds_CtxNegative(ctx, MICROUDS_NRC_IK);
}

/* ======================================================================
 *                              服务表
 * ====================================================================== */

static const MicroUds_ServiceEntry_t *MicroUds_FindService(uint8_t sid)
{
    for (uint32_t i = 0; i < uds_obj.service.serv_num; i++)
    {
        if (uds_obj.service.serv[i].is_valid && uds_obj.service.serv[i].sid == sid)
        {
            return &uds_obj.service.serv[i];
        }
    }
    return NULL; // 先把整个骨架搭起来，什么查找算法的后续再说吧，先跑起来验证架构再说 (后续可由生成器排序后二分)
}

static const MicroUds_SubFunc_t *MicroUds_FindSubFunc(const MicroUds_ServiceEntry_t *serv, uint8_t id)
{
    for (uint32_t j = 0; j < serv->sub_func_num; j++)
    {
        if (serv->sub_func[j].id == id)
        {
            return &serv->sub_func[j];
        }
    }
    return NULL;
}

static void MicroUds_RegisterBuiltin(uint8_t sid, uint8_t flags, uint32_t session_mask, uint8_t permis,
                                     uint16_t min_len, uint16_t max_len, MicroUds_ServiceFunc_t cb)
{
    MicroUds_ServiceEntry_t *s = &uds_obj.service.serv[uds_obj.service.serv_num++];

    s->sid = sid;
    s->flags = (uint8_t)(flags | MICROUDS_SERV_FLAG_BUILTIN | MICROUDS_SERV_FLAG_HAS_SUBFUNC);
    s->session_mask = session_mask;
    s->permis = permis;
    s->min_len = min_len;
    s->max_len = max_len;
    s->callback = cb;
    s->Userdata = NULL;
    s->sub_func_num = 0;
    s->is_valid = true;
}

MicroUds_Status_t MicroUds_ServiceRegisterServ(const MicroUds_ServiceTable_t *table, size_t size)
{
    if (table == NULL || size == 0 || size > MICROUDS_SERVICE_MAX_NUMBER)
    {
        return MICROUDS_PARAM_ERR;
    }

    // [REVIEW] 原来每次注册都从 serv[0] 开始覆盖，第二次注册会冲掉第一次的服务(包括子功能)，而且会把 service.active 清掉。
    //          现在是追加，并且先整体校验再提交，避免注册一半失败留下半张表。
    if (uds_obj.service.serv_num + size > MICROUDS_SERVICE_MAX_NUMBER)
    {
        return MICROUDS_PARAM_ERR;
    }

    for (size_t i = 0; i < size; i++)
    {
        if ((table[i].flags & MICROUDS_SERV_FLAG_BUILTIN) != 0u ||
            ((table[i].flags & MICROUDS_SERV_FLAG_SUPPRESS) != 0u && (table[i].flags & MICROUDS_SERV_FLAG_HAS_SUBFUNC) == 0u))
        {
            return MICROUDS_PARAM_ERR; // 抑制位只对有子功能的服务有意义
        }

        if (MicroUds_FindService(table[i].sid) != NULL) // 与已注册的(含内置的 0x10/0x27/0x3E)重复
        {
            return MICROUDS_PARAM_ERR;
        }

        for (size_t k = 0; k < i; k++) // 与本次表内的重复
        {
            if (table[k].sid == table[i].sid)
            {
                return MICROUDS_PARAM_ERR;
            }
        }
    }

    for (size_t i = 0; i < size; i++)
    {
        MicroUds_ServiceEntry_t *s = &uds_obj.service.serv[uds_obj.service.serv_num++];

        memset(s, 0, sizeof(MicroUds_ServiceEntry_t));
        s->sid = table[i].sid;
        s->flags = table[i].flags;
        s->session_mask = table[i].session_mask;
        s->permis = table[i].permis;
        s->min_len = table[i].min_len;
        s->max_len = table[i].max_len;
        s->callback = table[i].callback;
        s->Userdata = table[i].Userdata;
        s->is_valid = true;
    }

    return MICROUDS_OK;
}

MicroUds_Status_t MicroUds_ServiceRegisterSubFunc(uint8_t sid, const MicroUds_SubFuncTable_t *table, size_t size)
{
    if (table == NULL || size == 0 || size > MICROUDS_SUBFUNC_MAX_NUMBER)
    {
        return MICROUDS_PARAM_ERR;
    }

    MicroUds_ServiceEntry_t *service = NULL;

    for (uint32_t i = 0; i < uds_obj.service.serv_num; i++)
    {
        if (sid == uds_obj.service.serv[i].sid)
        {
            service = &uds_obj.service.serv[i];
            break;
        }
    }

    if (service == NULL)
    {
        return MICROUDS_NOT_FOUND;
    }

    // 内置服务的子功能由内核自己解析；没有子功能的服务(0x22 ...)注册子功能没有意义
    if ((service->flags & MICROUDS_SERV_FLAG_BUILTIN) != 0u || (service->flags & MICROUDS_SERV_FLAG_HAS_SUBFUNC) == 0u)
    {
        return MICROUDS_PARAM_ERR;
    }

    for (size_t i = 0; i < size; i++)
    {
        // 带抑制位的服务，子功能编号只有 7 位，bit7 被占用
        if ((service->flags & MICROUDS_SERV_FLAG_SUPPRESS) != 0u && (table[i].id & 0x80u) != 0u)
        {
            return MICROUDS_PARAM_ERR;
        }
        for (size_t k = 0; k < i; k++)
        {
            if (table[k].id == table[i].id)
            {
                return MICROUDS_PARAM_ERR;
            }
        }
    }

    for (size_t i = 0; i < size; i++)
    {
        service->sub_func[i] = table[i];
    }

    service->sub_func_num = (uint32_t)size;

    return MICROUDS_OK;
}

/* ======================================================================
 *                              服务分发
 * ====================================================================== */

/**
 * @brief 一个请求处理结束: 释放请求，重启S3
 */
static void MicroUds_RequestDone(void)
{
    uds_obj.service.pending = false;
    uds_obj.service.pending_cnt = 0;
    uds_obj.service.call_cnt = 0;
    uds_obj.service.cur = NULL;
    uds_obj.service.cur_sub = NULL;

    uds_obj.session.last_tick = uds_obj.tick; // S3 从响应处理完成起重新计时
    uds_obj.pdu.active = false;
}

static void MicroUds_Reject(uint8_t nrc)
{
    NegativeResponse(uds_obj.pdu.sid, nrc);
    MicroUds_RequestDone();
}

/**
 * @brief 调用服务回调并处理结果。首次调用和 PENDING 之后的重入都走这里
 */
static void MicroUds_ServiceRun(const MicroUds_ServiceEntry_t *serv, const MicroUds_SubFunc_t *sub)
{
    bool has_sub = (serv->flags & MICROUDS_SERV_FLAG_HAS_SUBFUNC) != 0u;
    uint32_t off = has_sub ? 2u : 1u;

    MicroUds_ServiceFunc_t cb = (sub != NULL) ? sub->callback : serv->callback;

    // [REVIEW] 原来 "找到子功能但 callback 为 NULL" 时既不响应也不清 active，请求会被每个周期重复处理且永远没有响应
    if (cb == NULL)
    {
        MicroUds_Reject(MICROUDS_NRC_GR);
        return;
    }

    MicroUds_ServiceCtx_t ctx = {0};

    // [REVIEW] 原来 ctx 全是 0，回调看不到请求内容，也拿不到注册时传的 Userdata
    ctx.sid = serv->sid;
    ctx.sub_func = uds_obj.pdu.sub_func;
    ctx.addr = uds_obj.pdu.addr;
    ctx.req = uds_obj.pdu.payload;
    ctx.req_len = uds_obj.pdu.len;
    ctx.session = uds_obj.session.ssid;
    ctx.sec_level = uds_obj.security.permis;
    ctx.call_cnt = uds_obj.service.call_cnt;
    ctx.Userdata = (sub != NULL) ? sub->Userdata : serv->Userdata;
    ctx.payload = &uds_obj.resp.data[off];
    ctx.payload_cap = MICROUDS_RESPONSE_BUF_SIZE - off;
    ctx.resp_type = MICROUDS_RESPTYPE_NEGATIVE; // 默认负响应，回调必须显式给出结果
    ctx.nrc = MICROUDS_NRC_GR;

    uds_obj.service.call_cnt++;
    cb(&ctx);

    switch (ctx.resp_type)
    {
    case MICROUDS_RESPTYPE_POSITIVE:
        if (PositiveResponse(serv->sid, has_sub, uds_obj.pdu.sub_func, ctx.payload, ctx.len) == MICROUDS_OK &&
            uds_obj.resp.type == MICROUDS_RESPTYPE_POSITIVE)
        {
            uds_obj.resp.apply_ssid = uds_obj.session.pending_ssid; // 0x10 成功: 响应发出后切会话
        }
        uds_obj.session.pending_ssid = 0;
        MicroUds_RequestDone();
        break;

    case MICROUDS_RESPTYPE_PENDING:
    {
        bool send = false;

        if (!uds_obj.service.pending)
        {
            uds_obj.service.pending = true;
            uds_obj.service.cur = serv;
            uds_obj.service.cur_sub = sub;
            uds_obj.service.pending_cnt = 0;
            send = true; // 首次: 立刻告诉诊断仪 "收到了，在处理"
        }
        else if (MICROUDS_TICK_ELAPSED(uds_obj.tick, uds_obj.service.pending_tick, MICROUDS_MS_TO_TICK(MICROUDS_PENDING_RESEND_MS)))
        {
            if (uds_obj.service.pending_cnt >= MICROUDS_PENDING_MAX_COUNT)
            {
                MicroUds_Reject(MICROUDS_NRC_GR); // 拖太久，内核强制结束
                break;
            }
            send = true; // 在 P2* 到期之前补发一次 0x78
        }

        if (send)
        {
            NegativeResponse(serv->sid, MICROUDS_NRC_RCRRP);
            uds_obj.service.pending_tick = uds_obj.tick;
            uds_obj.service.pending_cnt++;
        }
        uds_obj.session.pending_ssid = 0;
        break;
    }

    case MICROUDS_RESPTYPE_NEGATIVE:
    default:
        uds_obj.session.pending_ssid = 0;
        MicroUds_Reject((ctx.nrc == MICROUDS_NRC_PR) ? MICROUDS_NRC_GR : ctx.nrc); // NRC 0x00 不是合法负响应码
        break;
    }
}

/**
 * @brief 请求处理流水线
 * [REVIEW] 校验顺序: 服务是否存在(0x11) -> 功能寻址是否允许 -> 会话(0x7F) -> 能否读出子功能(0x13)
 *          -> 子功能是否存在(0x12)/当前会话是否允许(0x7E) -> 请求长度(0x13) -> 安全访问(0x33) -> 调用回调
 *          具体顺序请对照你们的OEM规范，集中在这一个函数里，改起来也方便。
 */
static void MicroUds_ServiceHandler(void)
{
    MicroUds_Pdu_t *pdu = &uds_obj.pdu;

    // [REVIEW] 上一条响应还没发出去(传输层忙)时不处理新请求，响应缓冲只有一份
    if (!pdu->active || uds_obj.resp.active)
    {
        return;
    }

    // PENDING 中的请求: 每个周期重入，直到回调给出最终结果
    if (uds_obj.service.pending)
    {
        MicroUds_ServiceRun(uds_obj.service.cur, uds_obj.service.cur_sub);
        return;
    }

    if (pdu->err_nrc != 0u) // RxIndication阶段发现的错误，如请求过长
    {
        MicroUds_Reject(pdu->err_nrc);
        return;
    }

    const MicroUds_ServiceEntry_t *serv = MicroUds_FindService(pdu->sid);

    if (serv == NULL || serv->session_mask == 0u)
    {
        MicroUds_Reject(MICROUDS_NRC_SNS); // 服务不支持 (功能寻址下发送阶段会静默)
        return;
    }

    if (pdu->addr == MICROUDS_ADDR_FUNC && (serv->flags & MICROUDS_SERV_FLAG_FUNC_ADDR) == 0u)
    {
        MicroUds_RequestDone(); // 该服务不接受功能寻址: 静默忽略
        return;
    }

    if ((serv->session_mask & MICROUDS_SET_SESSION(uds_obj.session.ssid)) == 0u)
    {
        MicroUds_Reject(MICROUDS_NRC_SNSIAS); // 服务在其他会话可用，当前会话不可用
        return;
    }

    // ---- 解析子功能 ----
    // [REVIEW] 原来在 RxIndication 里无条件把 data[1] 当子功能、当抑制位，对 0x22(data[1]是DID高字节)会误判。
    //          现在只有服务声明了 HAS_SUBFUNC / SUPPRESS 才这样解析。
    bool has_sub = (serv->flags & MICROUDS_SERV_FLAG_HAS_SUBFUNC) != 0u;

    pdu->suppress = false;
    if (has_sub)
    {
        if (pdu->raw_len < 2u)
        {
            MicroUds_Reject(MICROUDS_NRC_IMLOI); // [REVIEW] 原来 len<2 一律回 GR，且对所有服务一刀切；1字节的请求只有在服务需要子功能时才是长度错误
            return;
        }

        uint8_t sub = pdu->raw[1];

        if ((serv->flags & MICROUDS_SERV_FLAG_SUPPRESS) != 0u)
        {
            pdu->suppress = MICROUDS_GET_RESPINHIBIT(sub);
            sub = (uint8_t)(sub & 0x7Fu);
        }
        pdu->sub_func = sub;
        pdu->payload = &pdu->raw[2];
        pdu->len = pdu->raw_len - 2u;
    }
    else
    {
        pdu->sub_func = 0;
        pdu->payload = &pdu->raw[1];
        pdu->len = pdu->raw_len - 1u;
    }

    // ---- 子功能查找 ----
    const MicroUds_SubFunc_t *sub = NULL;

    if (has_sub && serv->sub_func_num > 0u)
    {
        sub = MicroUds_FindSubFunc(serv, pdu->sub_func);

        if (sub == NULL)
        {
            MicroUds_Reject(MICROUDS_NRC_SFNS); // 子功能全局不支持
            return;
        }

        if (sub->session_mask != 0u && (sub->session_mask & MICROUDS_SET_SESSION(uds_obj.session.ssid)) == 0u)
        {
            MicroUds_Reject(MICROUDS_NRC_SFNSIAS); // 子功能在其他会话可用，当前会话不可用
            return;
        }
    }

    // ---- 长度 ----
    if ((serv->min_len != 0u && pdu->raw_len < serv->min_len) || (serv->max_len != 0u && pdu->raw_len > serv->max_len))
    {
        MicroUds_Reject(MICROUDS_NRC_IMLOI);
        return;
    }

    // ---- 安全访问 ----
    if (!MicroUds_SecurityPass(serv->permis) || (sub != NULL && !MicroUds_SecurityPass(sub->permis)))
    {
        MicroUds_Reject(MICROUDS_NRC_SAD);
        return;
    }

    uds_obj.service.call_cnt = 0;
    MicroUds_ServiceRun(serv, sub);
}

/* ======================================================================
 *                              对外接口
 * ====================================================================== */

MicroUds_Status_t MicroUds_Init(void)
{
    memset(&uds_obj, 0, sizeof(MicroUds_Obj_t));
    uds_obj.session.mask = MICROUDS_SESSION_MASK_UPTO(MICROUDS_SESSION_MASK); // 支持 MICROUDS_SESSION_MASK 个会话 (会话1..N)
    uds_obj.session.ssid = MICROUDS_SESSION_DEFAULT;                          // 开局设置一个默认会话 (存编号，不是掩码)
    uds_obj.session.is_default = true;
    uds_obj.session.hold_tick = MICROUDS_SESSION_HOLD_TICK; // [REVIEW] 原来从没赋值(=0)，且 is_default 在 0x10 之后不会变false，S3 永远不会触发

    // 内核内置服务
    // 0x10: 所有会话可用，允许功能寻址，支持抑制位，请求长度固定 2 字节
    MicroUds_RegisterBuiltin(0x10, MICROUDS_SERV_FLAG_SUPPRESS | MICROUDS_SERV_FLAG_FUNC_ADDR,
                             uds_obj.session.mask, 0, 2, 2, MicroUds_SvcSessionControl);

    // 0x27: 仅扩展/编程会话可用 (原来只在扩展会话)，不允许功能寻址，不支持抑制位，请求长度 >= 2
    // [REVIEW] 不在允许的会话里应返回 0x7F ServiceNotSupportedInActiveSession，原来返回的是 0x22
    MicroUds_RegisterBuiltin(0x27, 0,
                             MICROUDS_SET_SESSION(MICROUDS_SESSION_PROGRAM) | MICROUDS_SET_SESSION(MICROUDS_SESSION_EXTEND),
                             0, 2, 0, MicroUds_SvcSecurityAccess);

    // 0x3E: 所有会话可用，允许功能寻址，支持抑制位 (3E 80)，请求长度固定 2 字节
    MicroUds_RegisterBuiltin(0x3E, MICROUDS_SERV_FLAG_SUPPRESS | MICROUDS_SERV_FLAG_FUNC_ADDR,
                             uds_obj.session.mask, 0, 2, 2, MicroUds_SvcTesterPresent);

    return MICROUDS_OK;
}

void MicroUds_TickHandler(void)
{
    uds_obj.tick++;
}

void MicroUds_TimerHandler(void)
{
    uint32_t current_tick = uds_obj.tick; // 取得当前tick

    // S3: 非默认会话下，空闲超过 hold_tick 回到默认会话
    // [REVIEW] 请求处理中(含PENDING)、响应还没发出时不计S3；回默认会话后统一走 SessionSwitch(会重新锁定安全)。
    //          原来的 "last_tick += hold_tick  避免周期漂移" 是周期定时器的写法，超时一次性事件不需要。
    if (uds_obj.session.ssid != MICROUDS_SESSION_DEFAULT && !uds_obj.pdu.active && !uds_obj.resp.active &&
        MICROUDS_TICK_ELAPSED(current_tick, uds_obj.session.last_tick, uds_obj.session.hold_tick))
    {
        MicroUds_SessionSwitch(MICROUDS_SESSION_DEFAULT);
    }

    // [REVIEW] 原来的 MicroUds_SecurityHandler 已并入服务分发(0x27 是内置服务)
    MicroUds_ServiceHandler();  // 服务管理
    MicroUds_ResponseHandler(); // 响应管理
}

MicroUds_Status_t MicroUds_RxIndication(const uint8_t *data, size_t len, MicroUds_AddrType_t addrType)
{
    // [REVIEW] 原来是 "if (len < 2 || data == NULL) { NegativeResponse(data[0], ...) }"
    //   1. data==NULL 时还去读 data[0]，直接空指针解引用
    //   2. len==1 的请求是合法PDU(比如只有SID)，该由服务流水线按"服务是否存在/是否需要子功能"决定回 0x11 还是 0x13，不能一律拒绝
    //   回答 "这里应该静默吗?": len==0 没有SID，构造不出负响应，静默丢弃。其余情况都该有明确响应
    if (data == NULL || len == 0u)
    {
        return MICROUDS_PARAM_ERR;
    }

    // [REVIEW] 原来每来一条请求就覆盖 pdu/resp 的全局字段(addr、Inhibit...)，上一条还没处理完时会串。
    //          现在一次只处理一条，忙则拒收。如果项目要求 "忙时回 0x21"，由传输层根据返回值决定。
    if (uds_obj.pdu.active)
    {
        return MICROUDS_BUSY;
    }

    MicroUds_Pdu_t *pdu = &uds_obj.pdu;

    pdu->addr = addrType;
    pdu->sid = data[0];
    pdu->sub_func = 0;
    pdu->suppress = false;
    pdu->err_nrc = 0;
    pdu->payload = NULL;
    pdu->len = 0;

    if (len > MICROUDS_REQUEST_BUF_SIZE)
    {
        pdu->raw_len = 1; // 放不下，只保留SID，流水线里回 0x13
        pdu->raw[0] = data[0];
        pdu->err_nrc = MICROUDS_NRC_IMLOI;
    }
    else
    {
        memcpy(pdu->raw, data, len);
        pdu->raw_len = (uint32_t)len;
    }

    // 收到请求后同步会话时间
    uds_obj.session.last_tick = uds_obj.tick;

    // 最后才置 active，保证 TimerHandler 看到 active 时请求内容已经完整 (RxIndication 可能在中断/另一个任务里)
    pdu->active = true;

    // [REVIEW] 回答 "DID有必要单独实现吗？唯一区别就是子功能id从一个字节变两个字节了?":
    //   0x22 没有子功能，请求是 22 <DID高> <DID低> [<DID高> <DID低> ...]，一条请求里可以带多个 DID，响应是 62 + (DID+数据)*N，
    //   所以不是"子功能变两字节"这么简单，而是"没有子功能 + 变长的标识符列表"。
    //   0x31 是 31 <01/02/03> <RID高> <RID低> [选项]，子功能(启动/停止/取结果)+2字节RID，子功能层依然适用，RID 在服务回调里解析。
    //   现在的做法: 把 0x22/0x2E 注册成 "没有 HAS_SUBFUNC 的服务"，在服务级回调里自己解析 DID。
    //   等 0x22/0x2E 写多了，再抽一层 DID 表(DID + 读写权限 + 回调)，由 0x22 内置服务统一遍历 DID 并检查权限。
    return MICROUDS_OK;
}

uint8_t MicroUds_GetSession(void)
{
    return uds_obj.session.ssid;
}

uint8_t MicroUds_GetSecurityLevel(void)
{
    return uds_obj.security.permis;
}

/**
 * @brief 弱函数，用户重写 重新配置了cmake,弱符号不报错了哈哈
 *
 * @param pud
 * @param len
 * @return MicroUds_Status_t
 */
MicroUds_Status_t __attribute__((weak)) MicroUds_Transmit(const uint8_t *pdu, uint32_t len)
{
    (void)pdu;
    (void)len;

    return MICROUDS_OK;
}

/**
 * @brief 弱函数，由用户重写
 * [REVIEW] 默认实现把长度置0(=没有实现)，内核会回 0x10，而不是原来 "什么都不做、len 保持原值" 的情况
 */
void __attribute__((weak)) MicroUds_SecurityReqSeed(uint8_t level, uint8_t *out, uint32_t *len) // 请求种子
{
    (void)level;
    (void)out;
    if (len != NULL)
    {
        *len = 0;
    }
}

/**
 * @brief 弱函数，用户重写
 *
 */
void __attribute__((weak)) MicroUds_SecurityGetKey(uint8_t level, const uint8_t *seed, uint32_t seed_len, uint8_t *key, uint32_t *key_len)
{
    (void)level;
    (void)seed;
    (void)seed_len;
    (void)key;
    if (key_len != NULL)
    {
        *key_len = 0; // 没有实现 -> 任何密钥都校验失败
    }
}