/**
 * @file MicroUdsServer.c
 * @author https://xfp23.github.io
 * @brief MicroUds 服务端内核
 * @version \ref MICROUDS_VERSION
 * @date 2026-10-21
 *
 * @copyright Copyright (c) 2026
 *
 * ============================================================================
 *  模块总览
 * ============================================================================
 *  只处理"完整的 UDS PDU"(分段/流控由 ISOTP 负责)。流程:
 *  收到请求 -> 校验 -> 调服务回调 -> 构造响应 -> 交给 ISOTP 发送 -> 发送完成后收尾。
 *
 *  【谁在什么上下文调用什么】
 *    MicroUdsServer_RxIndication()   ISOTP 收到完整请求时调用(可能在中断)。只做"拷贝请求 + 置 pdu.active"，不处理业务
 *    MicroUdsServer_TickHandler()    定时中断，tick++
 *    MicroUdsServer_TimerHandler()   主循环/任务里周期调用。内核所有状态的推进都在这里(S3、分发、发送、收尾)
 *    MicroUdsServer_HwRespTxDone()   ISOTP 发送完成通知(可能在中断)。只置标志 tx_done，真正的收尾仍在 TimerHandler
 *    这样做的原因: RxIndication/HwRespTxDone 可能在中断里，而 pdu/resp/service 这些状态在主循环里被读写，
 *    中断里只写"一个标志"或"一份拷贝+active"，避免两边同时改同一批变量。
 *
 *  【一个请求的生命周期】
 *    RxIndication            pdu.active = true                       (请求被接收)
 *      -> ServiceHandler     校验流水线 -> 服务回调                   (TimerHandler 里)
 *      -> 响应排队            resp.active = true, resp.final = true   (最终响应; 0x78 则 final=false)
 *      -> ResponseHandler    Transmit() 成功 -> resp.tx_wait = true   (等 ISOTP 发完)
 *      -> ResponseFinish     HwRespTxDone / 超时 / 被静默丢弃 / 发送出错
 *                              1) 应用待生效的会话切换(0x10)
 *                              2) 调用户钩子 OnResponseSent (复位/跳转bootloader等"发完响应才能做的事")
 *                              3) RequestDone: pdu.active = false, S3 重新计时
 *
 *  【关键状态变量】
 *    pdu.active        一个请求正在处理中(从收到到最终响应结束)，期间新请求返回 BUSY
 *    resp.active       响应缓冲被占用(从排队到发送结束)，期间不会调用服务回调
 *    resp.tx_wait      已交给 ISOTP，等待发送完成通知
 *    resp.final        这条响应是不是请求的最终响应。0x78(ResponsePending)不是，发完它请求还要继续
 *    service.pending   服务返回过 PENDING，每个周期被重新调用，直到给出最终结果
 *    service.cur/sub   最近一次被执行的服务/子功能(用于 PENDING 重入，以及 OnResponseSent 取上下文)
 *
 *  【几条必须保持的规则】
 *    1. 请求只在"最终响应结束"后释放，0x78 的发送完成不释放请求
 *    2. 响应有三种结束方式，都必须走 ResponseFinish: 发送完成 / 被静默丢弃(抑制位、功能寻址) / 发送失败或超时
 *    3. 会话切换必须在响应发出后才生效，否则回给诊断仪的响应还没发完，会话就变了
 *    4. 服务回调(含用户的)只通过 ctx 表达结果，不能直接构造响应或修改内核状态
 *
 *  TODO: 目前只支持单实例(MICROUDS_INSTANCE_NUM)
 */
#include "MicroUdsServer.h"

#if MICROUDS_SERVER_ENABLE

#if MICROUDS_DID_ENABLE
#include "MicroUdsServer_Did.h"
#endif

/* 内核唯一的运行时对象，文件内可见，外部只能通过对外接口访问 */
static MicroUdsServer_Obj_t server_obj = {0};

/* 前向声明: 下面这几个函数在定义之前就被引用 */
static MicroUds_Status_t NegativeResponse(uint8_t sid, uint8_t nrc);
static void MicroUdsServer_SecurityLock(void);
static void MicroUdsServer_SessionSwitch(uint8_t ssid);
static void MicroUdsServer_RequestDone(void);

/*
 * 设计备忘: RID(0x31) 目前没有做成内置模块。
 *   RID 很少时，用户自己注册 0x31 + 子功能表(01/02/03)，在回调里解析 RID 即可。
 *   RID 变多时再抽成独立模块(RID 表 + start/stop/result 回调 + 运行状态)，因为 RID 带"状态"
 *   (未启动/运行中/已完成/已停止)，"没启动就 stop/取结果 -> 0x24"、"回默认会话要终止运行中的例程"这些规则
 *   每个例程都一样，不适合让每个用户在回调里各写一遍。内置模块同样通过本文件的注册接口接入，两种方式不冲突。
 */

/* ======================================================================
 *                              响应构造
 *  PositiveResponse / NegativeResponse 只负责"把响应写进缓冲并排队"(resp.active=true)，
 *  真正的发送在 ResponseHandler 里。响应缓冲只有一份，已有响应在排队/发送时返回 BUSY。
 * ====================================================================== */

/**
 * @brief 构造正响应: RSID [+ 子功能] + payload
 *
 * @param sid     请求的 SID(响应 SID = sid + 0x40)
 * @param has_sub 该服务请求里是否带子功能字节。带的话要在响应里回显(50 03 .. / 67 01 <seed> / 7E 00)
 * @param sub     要回显的子功能(已去掉抑制位)
 * @param payload 响应数据。回调直接写进了响应缓冲时(ctx->payload 的默认指向)就在原位，不再拷贝
 * @param len     payload 长度
 *
 * @note 响应放不下时改发负响应 0x14。抑制位和寻址方式在这里就存进响应，
 *       因为发送要等到后面，那时 pdu 里的内容可能已经不是这条请求的了。
 */
static MicroUds_Status_t PositiveResponse(uint8_t sid, bool has_sub, uint8_t sub, const uint8_t *payload, uint32_t len)
{
    if (server_obj.resp.active)
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
        return NegativeResponse(sid, MICROUDS_NRC_RTL);
    }

    server_obj.resp.type = MICROUDS_RESPTYPE_POSITIVE;
    server_obj.resp.addr = server_obj.pdu.addr;
    server_obj.resp.Inhibit = server_obj.pdu.suppress ? 1u : 0u;
    server_obj.resp.apply_ssid = 0;

    server_obj.resp.data[0] = (uint8_t)(sid + 0x40u);
    if (has_sub)
    {
        server_obj.resp.data[1] = sub;
    }

    if (len > 0u && payload != &server_obj.resp.data[off])
    {
        memmove(&server_obj.resp.data[off], payload, len);
    }

    server_obj.resp.len = off + len;
    server_obj.resp.active = true;

    return MICROUDS_OK;
}

/**
 * @brief 构造负响应: 7F SID NRC
 *
 * @note 抑制位只抑制正响应，负响应不受影响(Inhibit 恒为 0)。
 */
static MicroUds_Status_t NegativeResponse(uint8_t sid, uint8_t nrc)
{
    if (server_obj.resp.active)
    {
        return MICROUDS_BUSY;
    }

    server_obj.resp.type = MICROUDS_RESPTYPE_NEGATIVE;
    server_obj.resp.addr = server_obj.pdu.addr;
    server_obj.resp.Inhibit = 0;
    server_obj.resp.apply_ssid = 0;

    server_obj.resp.data[0] = 0x7F;
    server_obj.resp.data[1] = sid;
    server_obj.resp.data[2] = nrc;

    server_obj.resp.len = 3;
    server_obj.resp.active = true;

    return MICROUDS_OK;
}

/**
 * @brief 把刚排队的响应标记为"最终响应"
 *
 * 最终响应结束(发完/被丢弃)后请求才会被释放。0x78 不调用它。
 */
static void MicroUdsServer_RespMarkFinal(void)
{
    if (server_obj.resp.active)
    {
        server_obj.resp.final = true;
    }
}

/**
 * @brief 功能寻址下需要静默的负响应
 *
 * 功能寻址是广播，"不是我的服务/参数"不应该让所有 ECU 都回一遍负响应，
 * 所以 0x11/0x12/0x31/0x7E/0x7F 在功能寻址下不发。功能寻址的正响应照常发送。
 * 具体集合以你们 OEM 规范为准。
 */
static bool MicroUdsServer_IsSilentNrc(uint8_t nrc)
{
    return (nrc == MICROUDS_NRC_SNS) || (nrc == MICROUDS_NRC_SFNS) || (nrc == MICROUDS_NRC_ROOR) ||
           (nrc == MICROUDS_NRC_SFNSIAS) || (nrc == MICROUDS_NRC_SNSIAS);
}

/* ======================================================================
 *                         响应发送与收尾
 * ====================================================================== */

/**
 * @brief 一条响应结束: 切会话 -> (最终响应)通知用户 -> 释放请求
 *
 * 这是"响应结束"的唯一出口，以下三种情况都会走到这里:
 *   1) ISOTP 通知发送完成(HwRespTxDone) / 等通知超时
 *   2) 响应被静默丢弃(正响应被抑制、功能寻址下的静默负响应)——根本没发送，也就没有完成通知
 *   3) Transmit 返回了错误(不是 BUSY)
 *
 * 处理顺序:
 *   1) 先取走收尾需要的信息再清空 resp(清空后响应内容就没了)
 *   2) 应用待生效的会话切换。放在用户钩子之前，钩子里看到的 ctx->session 已经是切换后的会话
 *      (例如 10 02: 切到编程会话后，钩子里再去跳转 bootloader)
 *   3) 只有"最终响应"才继续往下: 通知用户，然后释放请求。0x78 发完不释放，服务还要继续执行
 *   4) 用户钩子必须在 RequestDone 之前调用: 钩子里要读 pdu 里的请求内容，
 *      RequestDone 之后 pdu 已经交还给 RxIndication，可能马上被新请求覆盖
 *
 * 用户钩子只在"服务回调实际执行过"的请求上触发(service.cur != NULL)。
 * 在到达服务之前就被流水线拒绝的请求(未知SID、会话不允许、长度错误、安全不满足...)不触发。
 * 钩子对该服务的正响应和负响应都会触发，用 ctx->resp_type 区分，见 MicroUdsServer_OnResponseSent。
 */
static void MicroUdsServer_ResponseFinish(void)
{
    const bool final = server_obj.resp.final;
    const MicroUds_RespType_t resp_type = server_obj.resp.type;
    const uint8_t nrc = (resp_type == MICROUDS_RESPTYPE_NEGATIVE) ? server_obj.resp.data[2] : 0u;
    const uint8_t apply_ssid = server_obj.resp.apply_ssid;

    memset(&server_obj.resp, 0, sizeof(MicroUdsServer_Response_t));

    if (apply_ssid != 0u)
    {
        MicroUdsServer_SessionSwitch(apply_ssid);
    }

    if (!final)
    {
        return; // 0x78: 请求继续存活
    }

    const MicroUdsServer_ServiceEntry_t *serv = server_obj.service.cur;
    const MicroUdsServer_SubFunc_t *sub = server_obj.service.cur_sub;

    if (serv != NULL)
    {
        MicroUdsServer_ServiceCtx_t ctx = {0};

        ctx.sid = serv->sid;
        ctx.sub_func = server_obj.pdu.sub_func;
        ctx.addr = server_obj.pdu.addr;
        ctx.req = server_obj.pdu.payload;
        ctx.req_len = server_obj.pdu.len;
        ctx.session = server_obj.session.ssid;
        ctx.sec_level = server_obj.security.permis;
        ctx.call_cnt = server_obj.service.call_cnt; // 服务回调被调用的总次数
        ctx.Userdata = (sub != NULL) ? sub->Userdata : serv->Userdata;
        ctx.resp_type = resp_type; // 最终响应的类型: 正/负
        ctx.nrc = nrc;             // 负响应的 NRC，正响应为 0
        // payload/payload_cap/len 在这个钩子里没有意义(响应已经发完)，保持 NULL/0

        MicroUdsServer_OnResponseSent(&ctx);
    }

    MicroUdsServer_RequestDone();
}

/**
 * @brief 响应发送状态机，由 TimerHandler 每个周期调用
 *
 *   resp.active = false                 无响应，直接返回
 *   resp.tx_wait = true                 已交给 ISOTP，等 tx_done 标志或超时
 *   其他(resp.active && !tx_wait)       决定是丢弃还是发送:
 *       丢弃 -> ResponseFinish
 *       发送 -> Transmit()
 *                  OK   -> 进入 tx_wait
 *                  BUSY -> 保留响应，下个周期重试
 *                  其他 -> 丢弃，ResponseFinish
 */
static void MicroUdsServer_ResponseHandler(void)
{
    if (!server_obj.resp.active)
    {
        return;
    }

    // 已交给 ISOTP: 等发送完成通知。超时保护避免通知丢失时请求永远卡住
    if (server_obj.resp.tx_wait)
    {
        if (server_obj.tx_done)
        {
            server_obj.tx_done = false;
            MicroUdsServer_ResponseFinish();
        }
        else if (MICROUDS_TICK_ELAPSED(server_obj.tick, server_obj.resp.tx_tick, MICROUDS_MS_TO_TICK(MICROUDS_TX_CONFIRM_TIMEOUT_MS)))
        {
            MicroUdsServer_ResponseFinish();
        }
        return;
    }

    // 是否丢弃: 正响应看抑制位，负响应看(功能寻址 且 属于静默集合)
    bool drop = false;

    if (server_obj.resp.type == MICROUDS_RESPTYPE_POSITIVE)
    {
        drop = (server_obj.resp.Inhibit != 0u);
    }
    else
    {
        drop = (server_obj.resp.addr == MICROUDS_ADDR_FUNC) && MicroUdsServer_IsSilentNrc(server_obj.resp.data[2]);
    }

    if (!drop)
    {
        // 必须先进入 tx_wait 再调 Transmit: 如果 Transmit 内部同步调用了 HwRespTxDone，通知才不会丢
        server_obj.resp.tx_wait = true;
        server_obj.resp.tx_tick = server_obj.tick;
        server_obj.tx_done = false;

        MicroUds_Status_t st = MicroUdsServer_RespTransmit((const uint8_t *)server_obj.resp.data, server_obj.resp.len);

        if (st == MICROUDS_OK)
        {
            return;
        }

        server_obj.resp.tx_wait = false;

        if (st == MICROUDS_BUSY)
        {
            return; // 传输层忙，下个周期重试
        }
        // 其他错误: 这条响应丢弃(不会有完成通知)，直接结束。TODO: 加重试次数/错误回调
    }

    MicroUdsServer_ResponseFinish();
}

/* ======================================================================
 *                              会话 / 安全 状态
 * ====================================================================== */

/**
 * @brief 锁定安全访问: 回到未解锁状态，作废未完成的种子
 *
 * 只清"解锁状态"和"进行中的种子/密钥"，attempts/delay_* 故意不清:
 * 否则诊断仪可以靠等 S3 超时、切会话来重置失败计数，绕过防暴力破解。
 */
static void MicroUdsServer_SecurityLock(void)
{
    server_obj.security.permis = 0;
    server_obj.security.level = 0;
    server_obj.security.step = MICROUDS_SEC_STEP_IDLE;
    server_obj.security.seed_len = 0;
    memset(server_obj.security.seed, 0, sizeof(server_obj.security.seed));
    memset(server_obj.security.key, 0, sizeof(server_obj.security.key));
    memset(server_obj.security.key_len, 0, sizeof(server_obj.security.key_len));
}

/**
 * @brief 会话切换的唯一入口(0x10 响应发完后、S3 超时回默认会话都走这里)
 *
 * 会话发生变化时: 重新锁定安全访问、通知 DID 模块、重启 S3。
 * 切到"同一个会话"不锁定安全 —— 各 OEM 规范对此不一致，需要时改这里。
 */
static void MicroUdsServer_SessionSwitch(uint8_t ssid)
{
    bool changed = (ssid != server_obj.session.ssid);

    if (changed)
    {
        MicroUdsServer_SecurityLock();
    }

    server_obj.session.ssid = ssid;
    server_obj.session.is_default = (ssid == MICROUDS_SESSION_DEFAULT);
    server_obj.session.last_tick = server_obj.tick;

#if MICROUDS_DID_ENABLE
    if (changed)
    {
        MicroUdsServer_Did_OnSessionChange(ssid); // 回默认会话时清除动态 DID
    }
#endif
}

/**
 * @brief 比较期望密钥(key[0])和诊断仪发来的密钥(key[1])
 *
 * 逐字节异或后累加再一次判断，比较耗时与密钥内容无关，避免时序侧信道。
 */
static bool MicroUdsServer_SecurityVerify(void)
{
    if (server_obj.security.key_len[0] != MICROUDS_KEY_SIZE || server_obj.security.key_len[1] != MICROUDS_KEY_SIZE)
    {
        return false;
    }

    uint8_t diff = 0;

    for (uint32_t i = 0; i < MICROUDS_KEY_SIZE; i++)
    {
        diff |= (uint8_t)(server_obj.security.key[0][i] ^ server_obj.security.key[1][i]);
    }
    return diff == 0u;
}

/**
 * @brief 服务/子功能的安全等级检查
 *
 * permis 字节: bit7 = 指定位，bit6..0 = 等级(用 MICROUDS_PERMIS_EXACT / MICROUDS_PERMIS_ATLEAST 生成)
 *   等级 0           无需解锁
 *   指定位=1         已解锁等级必须恰好等于要求等级
 *   指定位=0         已解锁等级 >= 要求等级即可
 */
static bool MicroUdsServer_SecurityPass(uint8_t permis)
{
    uint8_t need = MICROUDS_PERMIS_LEVEL(permis);

    if (need == 0u)
    {
        return true;
    }

    if (MICROUDS_GET_ASSIG(permis))
    {
        return server_obj.security.permis == need;
    }

    return server_obj.security.permis >= need;
}

/* ======================================================================
 *                         内核内置服务 0x10 / 0x27 / 0x3E
 *  它们和用户服务一样注册进服务表(见 MicroUdsServer_Init)，走同一条校验流水线
 *  (会话、长度、抑制位、安全、响应构造)，所以不会出现内置服务绕过校验的情况。
 *  回调里 ctx->sub_func 是内核已经解析好的子功能，ctx->req 是子功能之后的数据。
 * ====================================================================== */

/**
 * @brief 0x10 DiagnosticSessionControl
 *
 * 响应: 50 <会话> <P2 高 低> <P2* 高 低>，P2* 的单位是 10 ms。
 * 回调里只做校验并记下目标会话(pending_ssid)，不切换: 响应还没发出去，会话不能变。
 * ServiceRun 会把 pending_ssid 抄进 resp.apply_ssid，等响应结束时由 ResponseFinish 执行切换。
 */
static void MicroUdsServer_SvcSessionControl(MicroUdsServer_ServiceCtx_t *ctx)
{
    uint8_t target = ctx->sub_func;

    // 先判断范围再移位，避免 1 << target 在 target >= 32 时未定义
    if (target == 0u || target >= 32u || (server_obj.session.mask & MICROUDS_SET_SESSION(target)) == 0u)
    {
        MicroUdsServer_CtxNegative(ctx, MICROUDS_NRC_SFNS);
        return;
    }

    uint16_t p2 = (uint16_t)MICROUDS_P2_SERVER_MS;
    uint16_t p2s = (uint16_t)(MICROUDS_P2STAR_SERVER_MS / 10u);

    ctx->payload[0] = (uint8_t)(p2 >> 8);
    ctx->payload[1] = (uint8_t)(p2 & 0xFFu);
    ctx->payload[2] = (uint8_t)(p2s >> 8);
    ctx->payload[3] = (uint8_t)(p2s & 0xFFu);

    server_obj.session.pending_ssid = target;
    MicroUdsServer_CtxPositive(ctx, 4u);
}

/**
 * @brief 0x3E TesterPresent
 *
 * S3 在收到请求时已经刷新，这里只需要回应。3E 80 的"抑制正响应"由流水线统一处理。
 */
static void MicroUdsServer_SvcTesterPresent(MicroUdsServer_ServiceCtx_t *ctx)
{
    if (ctx->sub_func != 0x00u)
    {
        MicroUdsServer_CtxNegative(ctx, MICROUDS_NRC_SFNS);
        return;
    }
    MicroUdsServer_CtxPositive(ctx, 0u);
}

/**
 * @brief 0x27 SecurityAccess
 *
 * 子功能: 奇数 = requestSeed，偶数 = sendKey；27 01/02 对应等级1，27 03/04 对应等级2，依此类推。
 * 同步处理，一次请求一次响应:
 *   requestSeed:  检查失败锁定延时(0x37) -> 该等级已解锁则回全0种子 -> 调用户 SecurityReqSeed 生成种子 -> 回种子
 *   sendKey:      必须紧跟同等级的 requestSeed(否则 0x24) -> 校验长度(0x13) -> 调用户 SecurityGetKey 算期望密钥
 *                 -> 比较。成功: 解锁；失败: 0x35，连续失败次数达到上限后 0x36 并开始延时
 * 一次 sendKey 之后种子立即作废，无论成功与否(防止对同一个种子反复试)。
 */
static void MicroUdsServer_SvcSecurityAccess(MicroUdsServer_ServiceCtx_t *ctx)
{
    MicroUdsServer_Security_t *sec = &server_obj.security;
    uint8_t sub = ctx->sub_func;

    if (sub == 0u || sub > (uint8_t)(2u * MICROUDS_SECURITY_MAX_LEVEL))
    {
        MicroUdsServer_CtxNegative(ctx, MICROUDS_NRC_SFNS);
        return;
    }

    uint8_t level = (uint8_t)((sub + 1u) / 2u);

    if ((sub & 1u) != 0u)
    {
        /* ---------- requestSeed ---------- */
        if (sec->delay_active)
        {
            if (!MICROUDS_TICK_ELAPSED(server_obj.tick, sec->delay_start, MICROUDS_MS_TO_TICK(MICROUDS_SECURITY_DELAY_MS)))
            {
                MicroUdsServer_CtxNegative(ctx, MICROUDS_NRC_RTDSNE);
                return;
            }
            sec->delay_active = false; // 延时结束，重新给机会
            sec->attempts = 0;
        }

        if (ctx->payload_cap < MICROUDS_SEED_SIZE)
        {
            MicroUdsServer_CtxNegative(ctx, MICROUDS_NRC_RTL);
            return;
        }

        if (sec->permis == level) // 该等级已解锁: 按标准回全 0 种子
        {
            memset(ctx->payload, 0, MICROUDS_SEED_SIZE);
            MicroUdsServer_CtxPositive(ctx, MICROUDS_SEED_SIZE);
            return;
        }

        uint32_t seed_len = MICROUDS_SEED_SIZE; // [入] 缓冲容量 [出] 用户实际生成的长度
        memset(sec->seed, 0, sizeof(sec->seed));
        MicroUdsServer_SecurityReqSeed(level, sec->seed, &seed_len);

        if (seed_len == 0u || seed_len > MICROUDS_SEED_SIZE) // 用户没实现(默认弱函数返回0)或长度非法
        {
            sec->step = MICROUDS_SEC_STEP_IDLE;
            MicroUdsServer_CtxNegative(ctx, MICROUDS_NRC_GR);
            return;
        }

        sec->seed_len = seed_len;
        sec->level = level;
        sec->step = MICROUDS_SEC_STEP_SEED_SENT;

        memcpy(ctx->payload, sec->seed, seed_len);
        MicroUdsServer_CtxPositive(ctx, seed_len);
        return;
    }

    /* ---------- sendKey ---------- */
    if (sec->step != MICROUDS_SEC_STEP_SEED_SENT || sec->level != level)
    {
        MicroUdsServer_CtxNegative(ctx, MICROUDS_NRC_RSE);
        return;
    }

    if (ctx->req_len != MICROUDS_KEY_SIZE)
    {
        MicroUdsServer_CtxNegative(ctx, MICROUDS_NRC_IMLOI); // 种子保留，诊断仪可以重发
        return;
    }

    memcpy(sec->key[1], ctx->req, MICROUDS_KEY_SIZE); // key[1] = 诊断仪发来的密钥
    sec->key_len[1] = MICROUDS_KEY_SIZE;

    sec->key_len[0] = MICROUDS_KEY_SIZE; // [入] 缓冲容量 [出] 用户实际算出的长度; key[0] = 期望密钥
    memset(sec->key[0], 0, MICROUDS_KEY_SIZE);
    MicroUdsServer_SecurityGetKey(level, sec->seed, sec->seed_len, sec->key[0], &sec->key_len[0]);

    bool ok = MicroUdsServer_SecurityVerify();

    sec->step = MICROUDS_SEC_STEP_IDLE;
    memset(sec->seed, 0, sizeof(sec->seed));
    sec->seed_len = 0;
    memset(sec->key, 0, sizeof(sec->key));

    if (ok)
    {
        sec->permis = level;
        sec->attempts = 0;
        MicroUdsServer_CtxPositive(ctx, 0u);
        return;
    }

    sec->attempts++;
    if (sec->attempts >= MICROUDS_SECURITY_MAX_ATTEMPTS)
    {
        sec->delay_active = true;
        sec->delay_start = server_obj.tick;
        MicroUdsServer_CtxNegative(ctx, MICROUDS_NRC_ENOA);
        return;
    }
    MicroUdsServer_CtxNegative(ctx, MICROUDS_NRC_IK);
}

/* ======================================================================
 *                              服务表
 * ====================================================================== */

/**
 * @brief 按 SID 查找服务。目前线性查找，服务不多时足够；以后可由生成器排序后改二分
 */
static const MicroUdsServer_ServiceEntry_t *MicroUdsServer_FindService(uint8_t sid)
{
    for (uint32_t i = 0; i < server_obj.service.serv_num; i++)
    {
        if (server_obj.service.serv[i].is_valid && server_obj.service.serv[i].sid == sid)
        {
            return &server_obj.service.serv[i];
        }
    }
    return NULL;
}

/**
 * @brief 在服务的子功能表里查找子功能(表是用户用 ServiceRegisterSubFunc 注册的)
 */
static const MicroUdsServer_SubFunc_t *MicroUdsServer_FindSubFunc(const MicroUdsServer_ServiceEntry_t *serv, uint8_t id)
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

/**
 * @brief 注册内核内置服务: 请求里带子功能字节(0x10 / 0x27 / 0x3E / 0x2C)
 *
 * HAS_SUBFUNC 的含义是"请求第2字节是子功能字节"，内核据此解析子功能、处理抑制位、在正响应里回显子功能。
 * 它和"有没有注册子功能表"是两回事: 这些内置服务不需要子功能表，由服务级回调自己看 ctx->sub_func。
 */
static void MicroUdsServer_RegisterBuiltin(uint8_t sid, uint8_t flags, uint32_t session_mask, uint8_t permis,
                                     uint16_t min_len, uint16_t max_len, MicroUdsServer_ServiceFunc_t cb)
{
    MicroUdsServer_ServiceEntry_t *s = &server_obj.service.serv[server_obj.service.serv_num++];

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

#if MICROUDS_DID_ENABLE
/**
 * @brief 注册内核内置服务: 请求里没有子功能字节(0x22 / 0x2E)
 *
 * 只给确实没有子功能字节的服务用。不能用它注册 0x10 这类有子功能字节的服务:
 * 不带 HAS_SUBFUNC 时内核不解析子功能，ctx->sub_func 恒为 0，响应里也不会回显子功能。
 */
static void MicroUdsServer_NoSubRegisterBuiltin(uint8_t sid, uint8_t flags, uint32_t session_mask, uint8_t permis,
                                     uint16_t min_len, uint16_t max_len, MicroUdsServer_ServiceFunc_t cb)
{
    MicroUdsServer_ServiceEntry_t *s = &server_obj.service.serv[server_obj.service.serv_num++];

    s->sid = sid;
    s->flags = (uint8_t)(flags | MICROUDS_SERV_FLAG_BUILTIN);
    s->session_mask = session_mask;
    s->permis = permis;
    s->min_len = min_len;
    s->max_len = max_len;
    s->callback = cb;
    s->Userdata = NULL;
    s->sub_func_num = 0;
    s->is_valid = true;
}
#endif

/**
 * @brief 注册用户服务(追加)
 *
 * 先整体校验再提交: 任何一项不合法(容量、SID 与内置/已注册/本表内重复、标志位非法)都返回 PARAM_ERR，且一项都不注册。
 * 内置的 0x10/0x27/0x3E 和 DID 模块的 0x22/0x2E/0x2C 已占用，不能重复注册。
 */
MicroUds_Status_t MicroUdsServer_ServiceRegisterServ(const MicroUdsServer_ServiceTable_t *table, size_t size)
{
    if (table == NULL || size == 0 || size > MICROUDS_SERVICE_MAX_NUMBER)
    {
        return MICROUDS_PARAM_ERR;
    }

    if (server_obj.service.serv_num + size > MICROUDS_SERVICE_MAX_NUMBER)
    {
        return MICROUDS_PARAM_ERR;
    }

    for (size_t i = 0; i < size; i++)
    {
        if ((table[i].flags & MICROUDS_SERV_FLAG_BUILTIN) != 0u ||
            ((table[i].flags & MICROUDS_SERV_FLAG_SUPPRESS) != 0u && (table[i].flags & MICROUDS_SERV_FLAG_HAS_SUBFUNC) == 0u))
        {
            return MICROUDS_PARAM_ERR; // BUILTIN 用户不能设；抑制位只对有子功能字节的服务有意义
        }

        if (MicroUdsServer_FindService(table[i].sid) != NULL)
        {
            return MICROUDS_PARAM_ERR;
        }

        for (size_t k = 0; k < i; k++)
        {
            if (table[k].sid == table[i].sid)
            {
                return MICROUDS_PARAM_ERR;
            }
        }
    }

    for (size_t i = 0; i < size; i++)
    {
        MicroUdsServer_ServiceEntry_t *s = &server_obj.service.serv[server_obj.service.serv_num++];

        memset(s, 0, sizeof(MicroUdsServer_ServiceEntry_t));
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

/**
 * @brief 给已注册的、带子功能字节的用户服务注册子功能表(替换该服务原有的表)
 *
 * 注册之后，请求的子功能必须在表里: 不在表里 -> 0x12；在表里但当前会话不允许 -> 0x7E；
 * 每个子功能可以有自己的回调、会话和安全等级(见 MicroUdsServer_SubFuncTable_t)。
 * 内置服务和没有子功能字节的服务不能注册子功能表。
 */
MicroUds_Status_t MicroUdsServer_ServiceRegisterSubFunc(uint8_t sid, const MicroUdsServer_SubFuncTable_t *table, size_t size)
{
    if (table == NULL || size == 0 || size > MICROUDS_SUBFUNC_MAX_NUMBER)
    {
        return MICROUDS_PARAM_ERR;
    }

    MicroUdsServer_ServiceEntry_t *service = NULL;

    for (uint32_t i = 0; i < server_obj.service.serv_num; i++)
    {
        if (sid == server_obj.service.serv[i].sid)
        {
            service = &server_obj.service.serv[i];
            break;
        }
    }

    if (service == NULL)
    {
        return MICROUDS_NOT_FOUND;
    }

    if ((service->flags & MICROUDS_SERV_FLAG_BUILTIN) != 0u || (service->flags & MICROUDS_SERV_FLAG_HAS_SUBFUNC) == 0u)
    {
        return MICROUDS_PARAM_ERR;
    }

    for (size_t i = 0; i < size; i++)
    {
        // 带抑制位的服务，子功能编号只有 7 位，bit7 被抑制位占用
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
 * @brief 释放请求: 清掉服务执行状态，S3 重新计时，pdu 交还给 RxIndication
 *
 * 不要在服务回调返回后立刻调用: 响应此时只是排进了队列，还没发出去。
 * 它只会从两处被调用:
 *   - MicroUdsServer_ResponseFinish    最终响应结束之后(绝大多数情况)
 *   - MicroUdsServer_ServiceHandler    功能寻址请求被服务拒绝收、根本没有生成响应的静默路径
 */
static void MicroUdsServer_RequestDone(void)
{
    server_obj.service.pending = false;
    server_obj.service.pending_cnt = 0;
    server_obj.service.call_cnt = 0;
    server_obj.service.cur = NULL;
    server_obj.service.cur_sub = NULL;

    server_obj.session.last_tick = server_obj.tick; // S3 从这里开始重新计时
    server_obj.pdu.active = false;
}

/**
 * @brief 用负响应结束当前请求: 排队负响应并标记为最终响应(请求在它结束后释放)
 */
static void MicroUdsServer_Reject(uint8_t nrc)
{
    NegativeResponse(server_obj.pdu.sid, nrc);
    MicroUdsServer_RespMarkFinal();
}

/**
 * @brief 调用服务回调并处理结果。首次调用和 PENDING 之后的每次重入都走这里
 *
 * 回调通过 ctx->resp_type 给出结果:
 *   POSITIVE  排队正响应(最终)。0x10 成功时把 pending_ssid 记入 resp.apply_ssid，响应结束后才切会话
 *   PENDING   首次: 立即发 0x78 并进入 service.pending；之后每个周期重入，距上次 0x78 超过
 *             PENDING_RESEND_MS 再补发一次，次数超过 PENDING_MAX_COUNT 则强制以 0x10 结束
 *   NEGATIVE  排队负响应(最终)
 * 回调之前 ctx 默认是"负响应 + GR"，回调必须显式给出结果，避免漏设时误发正响应。
 */
static void MicroUdsServer_ServiceRun(const MicroUdsServer_ServiceEntry_t *serv, const MicroUdsServer_SubFunc_t *sub)
{
    // 请求里有没有子功能字节。决定正响应里要不要回显子功能(偏移 off): 有 -> RSID+子功能，没有 -> 只有 RSID
    bool has_sub = (serv->flags & MICROUDS_SERV_FLAG_HAS_SUBFUNC) != 0u;
    uint32_t off = has_sub ? 2u : 1u;

    // 用户给服务注册了子功能表时 sub != NULL，用该子功能自己的回调；否则(内置服务、无子功能的服务)用服务级回调
    MicroUdsServer_ServiceFunc_t cb = (sub != NULL) ? sub->callback : serv->callback;

    if (cb == NULL)
    {
        MicroUdsServer_Reject(MICROUDS_NRC_GR);
        return;
    }

    // 记录"正在/刚刚执行的服务": PENDING 重入需要它，响应结束后的 OnResponseSent 钩子也靠它取上下文
    server_obj.service.cur = serv;
    server_obj.service.cur_sub = sub;

    MicroUdsServer_ServiceCtx_t ctx = {0};

    ctx.sid = serv->sid;
    ctx.sub_func = server_obj.pdu.sub_func;
    ctx.addr = server_obj.pdu.addr;
    ctx.req = server_obj.pdu.payload;
    ctx.req_len = server_obj.pdu.len;
    ctx.session = server_obj.session.ssid;
    ctx.sec_level = server_obj.security.permis;
    ctx.call_cnt = server_obj.service.call_cnt;
    ctx.Userdata = (sub != NULL) ? sub->Userdata : serv->Userdata;
    ctx.payload = &server_obj.resp.data[off]; // 直接指向响应缓冲里 RSID(+子功能)之后，回调往里写，不需要再拷贝
    ctx.payload_cap = MICROUDS_RESPONSE_BUF_SIZE - off;
    ctx.resp_type = MICROUDS_RESPTYPE_NEGATIVE;
    ctx.nrc = MICROUDS_NRC_GR;

    server_obj.service.call_cnt++;
    cb(&ctx);

    switch (ctx.resp_type)
    {
    case MICROUDS_RESPTYPE_POSITIVE:
        if (PositiveResponse(serv->sid, has_sub, server_obj.pdu.sub_func, ctx.payload, ctx.len) == MICROUDS_OK &&
            server_obj.resp.type == MICROUDS_RESPTYPE_POSITIVE)
        {
            // 这里不切换会话，只是把"响应结束后要切到哪个会话"记在这条响应上。真正的切换见 ResponseFinish
            server_obj.resp.apply_ssid = server_obj.session.pending_ssid;
        }
        MicroUdsServer_RespMarkFinal();
        server_obj.session.pending_ssid = 0;
        break;

    case MICROUDS_RESPTYPE_PENDING:
    {
        bool send = false;

        if (!server_obj.service.pending)
        {
            server_obj.service.pending = true;
            server_obj.service.pending_cnt = 0;
            send = true;
        }
        else if (MICROUDS_TICK_ELAPSED(server_obj.tick, server_obj.service.pending_tick, MICROUDS_MS_TO_TICK(MICROUDS_PENDING_RESEND_MS)))
        {
            if (server_obj.service.pending_cnt >= MICROUDS_PENDING_MAX_COUNT)
            {
                MicroUdsServer_Reject(MICROUDS_NRC_GR);
                break;
            }
            send = true;
        }

        if (send)
        {
            NegativeResponse(serv->sid, MICROUDS_NRC_RCRRP); // 不标记 final: 0x78 发完后请求继续存活
            server_obj.service.pending_tick = server_obj.tick;
            server_obj.service.pending_cnt++;
        }
        server_obj.session.pending_ssid = 0;
        break;
    }

    case MICROUDS_RESPTYPE_NEGATIVE:
    default:
        server_obj.session.pending_ssid = 0;
        MicroUdsServer_Reject((ctx.nrc == MICROUDS_NRC_PR) ? MICROUDS_NRC_GR : ctx.nrc); // NRC 0x00 不是合法负响应码
        break;
    }
}

/**
 * @brief 请求分发流水线，TimerHandler 每个周期调用
 *
 * 校验顺序(任一步失败即以对应 NRC 结束，具体顺序请对照 OEM 规范，集中在这里方便修改):
 *   服务是否存在(0x11) -> 功能寻址是否允许(不允许则静默) -> 服务在当前会话是否可用(0x7F)
 *   -> 能否读出子功能字节(0x13) -> 子功能表: 是否存在(0x12)/当前会话是否允许(0x7E)
 *   -> 请求长度(0x13) -> 安全访问(0x33) -> 服务回调
 *
 * 不处理新请求的情况: 没有待处理请求；或响应缓冲还被上一条响应占用(排队/发送中/等完成通知)。
 * 后者同时保证了: PENDING 的服务不会在 0x78 还没发完时被重复调用。
 */
static void MicroUdsServer_ServiceHandler(void)
{
    MicroUdsServer_Pdu_t *pdu = &server_obj.pdu;

    if (!pdu->active || server_obj.resp.active)
    {
        return;
    }

    // 已经在 PENDING 的请求: 直接重入服务回调，不再校验
    if (server_obj.service.pending)
    {
        MicroUdsServer_ServiceRun(server_obj.service.cur, server_obj.service.cur_sub);
        return;
    }

    if (pdu->err_nrc != 0u) // RxIndication 阶段发现的错误，如请求超过缓冲
    {
        MicroUdsServer_Reject(pdu->err_nrc);
        return;
    }

    const MicroUdsServer_ServiceEntry_t *serv = MicroUdsServer_FindService(pdu->sid);

    if (serv == NULL || serv->session_mask == 0u)
    {
        MicroUdsServer_Reject(MICROUDS_NRC_SNS); // 功能寻址下会在发送阶段被静默丢弃
        return;
    }

    if (pdu->addr == MICROUDS_ADDR_FUNC && (serv->flags & MICROUDS_SERV_FLAG_FUNC_ADDR) == 0u)
    {
        MicroUdsServer_RequestDone(); // 服务不接受功能寻址: 静默忽略，没有响应，直接释放请求
        return;
    }

    if ((serv->session_mask & MICROUDS_SET_SESSION(server_obj.session.ssid)) == 0u)
    {
        MicroUdsServer_Reject(MICROUDS_NRC_SNSIAS);
        return;
    }

    // ---- 解析子功能字节 ----
    // 只有声明了 HAS_SUBFUNC/SUPPRESS 的服务才这样解析。0x22 这类服务的第2字节是 DID 高字节，不能当子功能/抑制位
    bool has_sub = (serv->flags & MICROUDS_SERV_FLAG_HAS_SUBFUNC) != 0u;

    pdu->suppress = false;
    if (has_sub)
    {
        if (pdu->raw_len < 2u) // 只有 SID 没有子功能字节(如单独一个 "10")
        {
            MicroUdsServer_Reject(MICROUDS_NRC_IMLOI);
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

    // ---- 子功能表 ----
    // 只有用户给这个服务注册了子功能表(sub_func_num > 0)才查表。
    // 内置服务(0x10 等)没有子功能表，这一段整体跳过，sub 保持 NULL，由服务级回调通过 ctx->sub_func 自行判断。
    const MicroUdsServer_SubFunc_t *sub = NULL;

    if (has_sub && serv->sub_func_num > 0u)
    {
        sub = MicroUdsServer_FindSubFunc(serv, pdu->sub_func);

        if (sub == NULL)
        {
            MicroUdsServer_Reject(MICROUDS_NRC_SFNS); // 子功能全局不支持
            return;
        }

        if (sub->session_mask != 0u && (sub->session_mask & MICROUDS_SET_SESSION(server_obj.session.ssid)) == 0u)
        {
            MicroUdsServer_Reject(MICROUDS_NRC_SFNSIAS); // 子功能在其他会话可用，当前会话不可用
            return;
        }
    }

    // ---- 请求长度(完整 PDU，含 SID) ----
    if ((serv->min_len != 0u && pdu->raw_len < serv->min_len) || (serv->max_len != 0u && pdu->raw_len > serv->max_len))
    {
        MicroUdsServer_Reject(MICROUDS_NRC_IMLOI);
        return;
    }

    // ---- 安全访问: 服务级 + 子功能级 都要满足 ----
    if (!MicroUdsServer_SecurityPass(serv->permis) || (sub != NULL && !MicroUdsServer_SecurityPass(sub->permis)))
    {
        MicroUdsServer_Reject(MICROUDS_NRC_SAD);
        return;
    }

    server_obj.service.call_cnt = 0;
    MicroUdsServer_ServiceRun(serv, sub);
}

/* ======================================================================
 *                              对外接口
 * ====================================================================== */

/**
 * @brief 初始化内核并注册内置服务
 *
 * 内置服务及其"服务级"条件:
 *   0x10  所有会话  功能寻址OK  支持抑制位  长度固定2
 *   0x27  编程/扩展会话  不允许功能寻址  不支持抑制位  长度>=2
 *   0x3E  所有会话  功能寻址OK  支持抑制位  长度固定2
 *   0x22/0x2E  所有会话  不允许功能寻址  无子功能字节   (DID 模块)
 *   0x2C       编程/扩展会话  支持抑制位  有子功能字节   (DID 模块)
 * DID 模块的服务级条件取各 DID 条件的并集(会话放开、安全不设限)，真正的权限在每个 DID 上判断(见 MicroUdsServer_Did.c)。
 * 服务级长度只限制下限，上限由回调判断: 0x22 的 DID 个数超限要回 0x31 而不是 0x13。
 * 用户的服务/DID 表必须在本函数之后注册。
 */
MicroUds_Status_t MicroUdsServer_Init(void)
{
    memset(&server_obj, 0, sizeof(MicroUdsServer_Obj_t));
    server_obj.session.mask = MICROUDS_SESSION_MASK_UPTO(MICROUDS_SESSION_MASK); // 支持会话 1..MICROUDS_SESSION_MASK
    server_obj.session.ssid = MICROUDS_SESSION_DEFAULT; // 上电进默认会话。ssid 永远存"会话编号"，不是位掩码
    server_obj.session.is_default = true;
    server_obj.session.hold_tick = MICROUDS_SESSION_HOLD_TICK;

    MicroUdsServer_RegisterBuiltin(0x10, MICROUDS_SERV_FLAG_SUPPRESS | MICROUDS_SERV_FLAG_FUNC_ADDR,
                             server_obj.session.mask, 0, 2, 2, MicroUdsServer_SvcSessionControl);

    MicroUdsServer_RegisterBuiltin(0x27, 0,
                             MICROUDS_SET_SESSION(MICROUDS_SESSION_PROGRAM) | MICROUDS_SET_SESSION(MICROUDS_SESSION_EXTEND),
                             0, 2, 0, MicroUdsServer_SvcSecurityAccess);

    MicroUdsServer_RegisterBuiltin(0x3E, MICROUDS_SERV_FLAG_SUPPRESS | MICROUDS_SERV_FLAG_FUNC_ADDR,
                             server_obj.session.mask, 0, 2, 2, MicroUdsServer_SvcTesterPresent);

#if MICROUDS_DID_ENABLE
    if (MicroUdsServer_Did_Init() != MICROUDS_OK)
    {
        return MICROUDS_ERR;
    }

    MicroUdsServer_NoSubRegisterBuiltin(0x22, 0 ,MICROUDS_SET_SESSION(MICROUDS_SESSION_PROGRAM) | MICROUDS_SET_SESSION(MICROUDS_SESSION_EXTEND) | MICROUDS_SET_SESSION(MICROUDS_SESSION_DEFAULT),
                            0,MICROUDS_DID_READ_REQ_MIN_LEN,0,MicroUdsServer_Did_0x22ReadHandler);

    MicroUdsServer_NoSubRegisterBuiltin(0x2E, 0 ,MICROUDS_SET_SESSION(MICROUDS_SESSION_PROGRAM) | MICROUDS_SET_SESSION(MICROUDS_SESSION_EXTEND) | MICROUDS_SET_SESSION(MICROUDS_SESSION_DEFAULT),
                            0,MICROUDS_DID_WRITE_REQ_MIN_LEN,0,MicroUdsServer_Did_0x2EWriteHandler);

    // 0x2C 请求里有子功能字节(01/02/03)，所以用带 HAS_SUBFUNC 的注册方式，子功能和抑制位由内核解析
    MicroUdsServer_RegisterBuiltin(0x2C, MICROUDS_SERV_FLAG_SUPPRESS,
                            MICROUDS_SET_SESSION(MICROUDS_SESSION_PROGRAM) | MICROUDS_SET_SESSION(MICROUDS_SESSION_EXTEND),
                            0,MICROUDS_DID_DYNA_REQ_MIN_LEN,0,MicroUdsServer_Did_0x2CDynamicHandler);
#endif

    return MICROUDS_OK;
}

/**
 * @brief 心跳，频率必须与 MICROUDS_FREQ_HZ 一致，在定时中断里调用
 */
void MicroUdsServer_TickHandler(void)
{
    server_obj.tick++;
}

/**
 * @brief 内核主函数，主循环/任务里周期调用，周期不要明显大于 1 个 tick
 *
 * 先处理 S3，再分发请求，最后推进响应发送。
 */
void MicroUdsServer_TimerHandler(void)
{
    uint32_t current_tick = server_obj.tick;

    // S3: 非默认会话下空闲超时回默认会话。请求处理中(含 PENDING)和响应未结束时不计 S3
    if (server_obj.session.ssid != MICROUDS_SESSION_DEFAULT && !server_obj.pdu.active && !server_obj.resp.active &&
        MICROUDS_TICK_ELAPSED(current_tick, server_obj.session.last_tick, server_obj.session.hold_tick))
    {
        MicroUdsServer_SessionSwitch(MICROUDS_SESSION_DEFAULT);
    }

    MicroUdsServer_ServiceHandler();
    MicroUdsServer_ResponseHandler();
}

/**
 * @brief ISOTP 通知: 一条响应已发送完成(或发送失败)
 *
 * 方向是 "传输层 -> 内核"。和 MicroUdsServer_OnResponseSent(内核 -> 用户钩子) 不要混淆。
 * 可以在中断里调用: 这里只置标志，真正的收尾(切会话、调钩子、释放请求)在 TimerHandler 里做。
 * 发送失败时也建议调用；不调用的话内核会在 MICROUDS_TX_CONFIRM_TIMEOUT_MS 后自行结束这条响应。
 */
void MicroUdsServer_HwRespTxDone(void)
{
    server_obj.tx_done = true;
}

/**
 * @brief 接收一条完整的请求 PDU (ISOTP 调用)
 *
 * @return MICROUDS_OK         已接收
 *         MICROUDS_BUSY       上一个请求还没处理完(含响应还没发完)，本条被丢弃，是否重试/回 0x21 由传输层决定
 *         MICROUDS_PARAM_ERR  空指针或长度 0: 没有 SID，构造不出任何响应，直接丢弃
 *
 * 这里只做"拷贝 + 登记"，不处理业务。请求被拷贝进内核自己的缓冲，因为:
 * 返回后传输层的缓冲可能被复用，而 PENDING 的服务还要继续读请求。
 * len==1(只有 SID)是合法 PDU，交给分发流水线判断回 0x11 还是 0x13。
 */
MicroUds_Status_t MicroUdsServer_RxIndication(const uint8_t *data, size_t len, MicroUds_AddrType_t addrType)
{
    if (data == NULL || len == 0u)
    {
        return MICROUDS_PARAM_ERR;
    }

    if (server_obj.pdu.active)
    {
        return MICROUDS_BUSY;
    }

    MicroUdsServer_Pdu_t *pdu = &server_obj.pdu;

    pdu->addr = addrType;
    pdu->sid = data[0];
    pdu->sub_func = 0;
    pdu->suppress = false;
    pdu->err_nrc = 0;
    pdu->payload = NULL;
    pdu->len = 0;

    if (len > MICROUDS_REQUEST_BUF_SIZE)
    {
        pdu->raw_len = 1; // 放不下: 只保留 SID，由分发流水线回 0x13
        pdu->raw[0] = data[0];
        pdu->err_nrc = MICROUDS_NRC_IMLOI;
    }
    else
    {
        memcpy(pdu->raw, data, len);
        pdu->raw_len = (uint32_t)len;
    }

    server_obj.session.last_tick = server_obj.tick; // 收到请求就刷新 S3

    // active 必须最后置位: TimerHandler 看到 active 时，请求内容一定已经完整 (本函数可能在中断/另一个任务里)
    pdu->active = true;

    return MICROUDS_OK;
}

/** @brief 当前会话编号 */
uint8_t MicroUdsServer_GetSession(void)
{
    return server_obj.session.ssid;
}

/** @brief 当前已解锁的安全等级，0 = 未解锁 */
uint8_t MicroUdsServer_GetSecurityLevel(void)
{
    return server_obj.security.permis;
}

/* ======================================================================
 *                         用户需要重写的弱函数
 * ====================================================================== */

/**
 * @brief 把一条完整响应交给传输层(ISOTP)发送
 *
 * @return MICROUDS_OK   已接受，发送完成后请调用 MicroUdsServer_HwRespTxDone()
 *         MICROUDS_BUSY 传输层暂时忙，内核下个周期重试
 *         其他          发送失败，内核丢弃这条响应
 */
MicroUds_Status_t __attribute__((weak)) MicroUdsServer_RespTransmit(const uint8_t *pdu, uint32_t len)
{
    (void)pdu;
    (void)len;

    return MICROUDS_OK;
}

/**
 * @brief 生成种子(0x27 requestSeed)
 *
 * @param level 安全等级(27 01 -> 1，27 03 -> 2 ...)
 * @param out   种子输出缓冲，容量 MICROUDS_SEED_SIZE
 * @param len   [入] 缓冲容量 [出] 实际生成的长度。默认实现置 0，表示"没有实现"，内核会回 0x10
 */
void __attribute__((weak)) MicroUdsServer_SecurityReqSeed(uint8_t level, uint8_t *out, uint32_t *len)
{
    (void)level;
    (void)out;
    if (len != NULL)
    {
        *len = 0;
    }
}

/**
 * @brief 根据种子计算期望密钥(0x27 sendKey)
 *
 * @param key_len [入] 缓冲容量 [出] 实际密钥长度。默认实现置 0，表示"没有实现"，此时任何密钥都校验失败
 */
void __attribute__((weak)) MicroUdsServer_SecurityGetKey(uint8_t level, const uint8_t *seed, uint32_t seed_len, uint8_t *key, uint32_t *key_len)
{
    (void)level;
    (void)seed;
    (void)seed_len;
    (void)key;
    if (key_len != NULL)
    {
        *key_len = 0;
    }
}

/**
 * @brief 响应结束后的用户钩子: 做"必须在响应发出之后才能做的事"，如软件复位、跳转 bootloader
 *
 * 所有服务共用这一个钩子(内核内置的和用户注册的都一样)，在钩子里用 ctx->sid / ctx->sub_func 区分。
 *
 * 触发条件: 服务回调已经执行、并给出了"最终响应"(正响应或负响应)，且这条响应已经
 *           发送完成 / 超时 / 被静默丢弃(如 11 81 抑制正响应) / 发送出错。
 *           0x78(ResponsePending)不触发；被流水线在到达服务之前就拒绝的请求(未知 SID、会话不允许、
 *           长度错误、安全不满足等)不触发。
 * 执行上下文: TimerHandler(主循环/任务)里。可以不返回(复位/跳转)；如果返回，内核随后释放请求。
 *            钩子执行期间新请求仍然返回 BUSY。
 *
 * ctx 里有意义的字段(只读):
 *   sid, sub_func, addr, req/req_len(请求数据), session(10 xx 时已是切换后的会话), sec_level,
 *   call_cnt(服务回调被调用的总次数), Userdata,
 *   resp_type(最终响应是 POSITIVE 还是 NEGATIVE), nrc(负响应时的 NRC)
 * payload/payload_cap/len 在这里没有意义。
 *
 * !! 负响应也会触发钩子。复位/跳转之前必须先判断 ctx->resp_type == MICROUDS_RESPTYPE_POSITIVE，
 *    否则 "11 01 被安全等级/条件拒绝" 的负响应也会让 ECU 复位。
 *
 * 示例:
 *   void MicroUdsServer_OnResponseSent(MicroUdsServer_ServiceCtx_t *ctx)
 *   {
 *       if (ctx->resp_type != MICROUDS_RESPTYPE_POSITIVE) { return; }
 *       if (ctx->sid == 0x11 && ctx->sub_func == 0x01) { NVIC_SystemReset(); }
 *       if (ctx->sid == 0x10 && ctx->sub_func == 0x02) { JumpToBootloader(); }
 *   }
 */
void __attribute__((weak)) MicroUdsServer_OnResponseSent(MicroUdsServer_ServiceCtx_t *ctx)
{
    (void)ctx;
}

#endif
