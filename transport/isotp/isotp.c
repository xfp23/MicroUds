/**
* @file isotp.c
* @author https://xfp23.github.io

* @brief ISO-15765-2 (DoCAN) 传输层 — 严格符合标准的重写

* @date 2026/04/02

* 状态机
* ==============
* 发送状态
* ISOTP_TX_IDLE : 无活动传输
* ISOTP_TX_SF : 单帧已排队，等待 Isotp_SendSuccess()
* ISOTP_TX_FF : 已发送第一帧或已到达块边界；等待 FC
* N_Bs 定时器正在运行
* ISOTP_TX_FC : 已收到 FC(Wait)；N_Bs 定时器正在运行
* ISOTP_TX_CF : 正在发送连续帧； STmin 定时器运行中
*
* 接收状态
* ISOTP_RX_IDLE：无活动接收
* ISOTP_RX_FF：已收到 FF；FC 帧已组装并排队等待发送
*（状态转换到 RX_FC 或 RX_WAIT 发生在
* Isotp_Send_ResponseAddress 内部，帧实际传递给总线层之后）
*
* ISOTP_RX_FC：已发送 FC(CTS)；正在等待 CF；N_Cr 定时器运行中
* ISOTP_RX_WAIT：已发送 FC(Wait)；上层尚未释放流
* 每隔 MICROUDS_FC_WAITTIME 个时钟周期重新发送周期性 FC(Wait)
* ISOTP_RX_CF：已收到所有 CF；上层数据已准备就绪
*
* 定时器映射（ISO 15765-2 表 5）
* N_Ar 发送方发送 CAN 帧的时间（硬件确认超时）
* N_Ar 接收方发送 CAN 帧的时间（硬件确认超时）
* N_Bs 发送方收到 FC 之前的等待时间
* N_Br 接收方发送 FC 之前的内部处理时间（未实现）
* N_Cs 发送方 CF 之间的 STmin 强制执行时间（基于时钟周期）
* N_Cr 接收方收到下一个 CF 之前的等待时间
*/

#include "isotp.h"
#include "string.h"
#include "uds_trans_port.h"
#include "uds_core.h"

/* =========================================================================
 * Module-private instance
 * ========================================================================= */
static Isotp_Obj_t isotp_obj = {0};
static Isotp_Obj_t *const ctx = &isotp_obj;

/* =========================================================================
 * Forward declarations
 * ========================================================================= */
static void Isotp_ResetTx(void);
static void Isotp_ResetRx(void);

/* =========================================================================
 * Reset
 * ========================================================================= */

/**
 * @brief  复位发送状态机
 */
static void Isotp_ResetTx(void)
{
    ctx->tx_state = ISOTP_TX_IDLE;
    ctx->tx_packet.Isbusy = false;
    ctx->tx_packet.N_Bs = 0;
    ctx->tx_packet.FcWait_count = 0;
    ctx->tx_packet.cf_count = 0;
    ctx->tx_packet.index_len = 0;
    ctx->tx_packet.total_len = 0;
    ctx->tx_packet.SN = 0;
    ctx->tx_packet.last_tick = 0;
    memset(ctx->tx_packet.buffer, 0, MICROUDS_TX_BUFFER_MEMORY_SIZE);
    memset(ctx->tx.data, 0, sizeof(ctx->tx.data));
    ctx->tx.en = false;
    ctx->tx.len = 0;
    ctx->N_Ar.en = false;
}

/**
 * @brief  复位接收状态机
 *
 * @note: 不清除等待fc的标志，这个由上层决定
 */
static void Isotp_ResetRx(void)
{
    ctx->rx_state = ISOTP_RX_IDLE;
    ctx->rx_packet.CF.timeout = 0;
    ctx->rx_packet.CF.SN = 0;
    ctx->rx_packet.index_len = 0;
    ctx->rx_packet.total_len = 0;
    ctx->rx_packet.FC.wait_tick = 0;
    ctx->rx_packet.FC.wait_count = 0;
    memset(ctx->rx_packet.buffer, 0, MICROUDS_RX_BUFFER_MEMORY_SIZE);
}

/* =========================================================================
 * ISO 15765-2 timers
 * ========================================================================= */

/**
 * @brief N_Bs 定时器
 * 等待流控超时的时间，发送用
 */
static void Isotp_Timer_N_Bs(void)
{
    bool active = (ctx->tx_state == ISOTP_TX_FF) ||
                  (ctx->tx_state == ISOTP_TX_FC &&
                   ctx->tx_packet.FC.Fs == ISOTP_FS_WAIT);
    if (active)
    {
        if (++ctx->tx_packet.N_Bs >= MICROUDS_N_BS)
        {
            Isotp_ResetTx(); /* ISO 15765-2 §6.7.3 */
        }
    }
    else
    {
        ctx->tx_packet.N_Bs = 0;
    }
}

/**
 * @brief 接收时期连续帧与连续帧之间的超时
 * Active when: RX_FC (CTS sent, CFs expected).
 */
static void Isotp_Timer_N_Cr(void)
{
    if (ctx->rx_state == ISOTP_RX_FC)
    {
        if (++ctx->rx_packet.CF.timeout >= MICROUDS_N_CR)
        {
            Isotp_ResetRx();
        }
    }
}

/**
 * @brief 硬件发送完成定时器
 *
 * @note 由 Isotp_SendSuccess()（硬件中断/回调）清除。上层调用，如若不想清除，可以直接在发送完成后`Isotp_SendSuccess`调用此函数
 *
 */
static void Isotp_Timer_N_Ar(void)
{
    if (!ctx->N_Ar.en)
        return;
    if ((ctx->tick - ctx->N_Ar.last_tick) >= MICROUDS_N_AR)
    { // 本机发送超时
        Isotp_ResetRx();
        Isotp_ResetTx();
    }
}

/* =========================================================================
 * 检查本机状态，接收完成直接传给上层多帧数据
 * ========================================================================= */
static void Isotp_StateCheck(void)
{
    Isotp_Timer_N_Cr();

    if (ctx->rx_state == ISOTP_RX_CF)
    {
        ctx->Uplayer.tx.en = true;
        ctx->Uplayer.tx.len = ctx->rx_packet.total_len;
        ctx->rx_state = ISOTP_RX_IDLE;
    }
}

/**

* @brief 处理存储在 ctx->frame 中的一个传入 CAN 帧。

*
* 发件人为 US 的 FC 帧会被优先拦截，并且永远不会

* 到达接收状态机。

*/
static void Isotp_ReceiveCallback(void)
{
    uint8_t pci = (ctx->frame.data[0] & 0xF0u) >> 4u;
    uint8_t nibble = ctx->frame.data[0] & 0x0Fu;

    /* ------------------------------------------------------------------
    这里直接处理发送的时候接收流控的逻辑
     * ------------------------------------------------------------------ */
    if (pci == ISOTP_TYPE_FC &&
        (ctx->tx_state == ISOTP_TX_FF || ctx->tx_state == ISOTP_TX_FC))
    {
        uint8_t fs = nibble;

        if (fs == ISOTP_FS_WAIT)
        {
            if (++ctx->tx_packet.FcWait_count >= MICROUDS_FC_WAIT_MAX)
            {
                Isotp_ResetTx();
                goto frame_done;
            }
            ctx->tx_packet.FC.Fs = fs;
            ctx->tx_packet.FC.Bs = ctx->frame.data[1];
            ctx->tx_packet.FC.STmin = ctx->frame.data[2];
            ctx->tx_packet.N_Bs = 0;
            ctx->tx_state = ISOTP_TX_FC;
        }
        else if (fs == ISOTP_FS_CTS)
        {
            ctx->tx_packet.FcWait_count = 0;
            ctx->tx_packet.FC.Fs = fs;
            ctx->tx_packet.FC.Bs = ctx->frame.data[1];
            ctx->tx_packet.FC.STmin = ctx->frame.data[2];
            ctx->tx_packet.N_Bs = 0;
            ctx->tx_packet.cf_count = 0;
            ctx->tx_state = ISOTP_TX_CF;
        }
        else /* ISOTP_FS_OVER */
        {
            Isotp_ResetTx();
        }
        goto frame_done;
    }

    /**
     * @brief 多帧接收状态机
     *
     */
    switch (pci)
    {
    /* ---- Single Frame -------------------------------------------- */
    case ISOTP_TYPE_SF:
    {
        uint8_t sf_len = nibble;
        if (sf_len == 0u || sf_len > 7u || sf_len > (ctx->frame.len - 1u))
            goto frame_done; /* invalid, silently discard */

        memset(ctx->rx_packet.buffer, 0, MICROUDS_RX_BUFFER_MEMORY_SIZE);
        memcpy(ctx->rx_packet.buffer, &ctx->frame.data[1], sf_len);
        ctx->Uplayer.tx.en = true;
        ctx->Uplayer.tx.len = sf_len;
        break;
    }

    /* ---- First Frame --------------------------------------------- */
    case ISOTP_TYPE_FF:
    {

        if (!ctx->frame.isPhySical)
            goto frame_done;

        if (ctx->rx_state != ISOTP_RX_IDLE)
            goto frame_done;

        uint16_t total = ((uint16_t)(nibble) << 8u) | ctx->frame.data[1];
        if (total < 8u)
            goto frame_done;

        ctx->rx_packet.total_len = total;
        ctx->rx_packet.index_len = 6u;
        ctx->rx_packet.CF.SN = 0u;
        memset(ctx->rx_packet.buffer, 0, MICROUDS_RX_BUFFER_MEMORY_SIZE);
        memcpy(ctx->rx_packet.buffer, &ctx->frame.data[2], 6u);

        // 先直接发个流控
        if (total > MICROUDS_RX_BUFFER_MEMORY_SIZE)
        {
            ctx->rx_packet.FC.Flow_status = ISOTP_FS_OVER;
        }
        else if (ctx->rx_packet.FC.isWaitFs)
        {
            ctx->rx_packet.FC.Flow_status = ISOTP_FS_WAIT;
        }
        else
        {
            ctx->rx_packet.FC.Flow_status = ISOTP_FS_CTS;
        }

        ctx->rx_packet.FC.Bs = MICROUDS_BLOCK_SIZE;
        ctx->rx_packet.FC.STmin = MICROUDS_SEPARATION_TIME;

        ctx->tx.data[0] = (uint8_t)((ISOTP_TYPE_FC << 4u) |
                                    (ctx->rx_packet.FC.Flow_status & 0x0Fu));
        ctx->tx.data[1] = (uint8_t)ctx->rx_packet.FC.Bs;
        ctx->tx.data[2] = (uint8_t)ctx->rx_packet.FC.STmin;
        memset(&ctx->tx.data[3], MICROUDS_PADDING_BYTES, 5u);
        ctx->tx.en = true;
        ctx->tx.len = 8u;

        /*
         * 切换状态机
         */
        ctx->rx_state = ISOTP_RX_FF;
        break;
    }

    /* ---- Consecutive Frame --------------------------------------- */
    case ISOTP_TYPE_CF:
    {

        if (!ctx->frame.isPhySical)
            goto frame_done;

        if (ctx->rx_state != ISOTP_RX_FC)
            goto frame_done;

        ctx->rx_packet.CF.timeout = 0u;

        uint8_t sn = nibble;
        uint8_t expected = (ctx->rx_packet.CF.SN + 1u) & 0x0Fu;
        if (sn != expected) // sn不对，直接结束本次接收
        {
            Isotp_ResetRx();
            goto frame_done;
        }
        ctx->rx_packet.CF.SN = sn;

        uint16_t remain = ctx->rx_packet.total_len - ctx->rx_packet.index_len;
        uint8_t copy_len = (remain > 7u) ? 7u : (uint8_t)remain;
        memcpy(ctx->rx_packet.buffer + ctx->rx_packet.index_len,
               &ctx->frame.data[1], copy_len);
        ctx->rx_packet.index_len += copy_len;

        if (ctx->rx_packet.index_len >= ctx->rx_packet.total_len)
        {
            ctx->rx_state = ISOTP_RX_CF;
        }
        break;
    }

    default:
        break;
    }

frame_done:
    memset(ctx->frame.data, 0, sizeof(ctx->frame.data));
    ctx->frame.en = false;
    ctx->frame.len = 0u;
}

/**
 * @brief 发送状态机管理
 *
 */
static void Isotp_TxProcess(void)
{
    switch (ctx->tx_state)
    {
    case ISOTP_TX_FF:
    case ISOTP_TX_FC:
        Isotp_Timer_N_Bs(); // 等待流控定时器
        break;

    case ISOTP_TX_CF:
    {

        if ((ctx->tick - ctx->tx_packet.last_tick) < ctx->tx_packet.FC.STmin)
            return;

        ctx->tx_packet.last_tick = ctx->tick;

        if (ctx->tx_packet.index_len >= ctx->tx_packet.total_len)
        {
            Isotp_ResetTx();
            return;
        }

        ctx->tx_packet.SN++;
        if (ctx->tx_packet.SN > 0x0Fu)
            ctx->tx_packet.SN = 0x00u;

        ctx->tx.data[0] = (uint8_t)((ISOTP_TYPE_CF << 4u) |
                                    (ctx->tx_packet.SN & 0x0Fu));

        uint16_t remain = ctx->tx_packet.total_len - ctx->tx_packet.index_len;
        uint8_t payload = (remain > 7u) ? 7u : (uint8_t)remain;
        memset(&ctx->tx.data[1], MICROUDS_PADDING_BYTES, 7u);
        memcpy(&ctx->tx.data[1],
               &ctx->tx_packet.buffer[ctx->tx_packet.index_len], payload);
        ctx->tx_packet.index_len += payload;

        ctx->tx.en = true;
        ctx->tx.len = 8u;

        if (ctx->tx_packet.FC.Bs != 0u)
        {
            if (++ctx->tx_packet.cf_count >= ctx->tx_packet.FC.Bs)
            {
                ctx->tx_state = ISOTP_TX_FF;
                ctx->tx_packet.N_Bs = 0u;
                ctx->tx_packet.cf_count = 0u;
            }
        }
        break;
    }

    default:
        break;
    }
}

/**
 * @brief 真正的发送函数
 *
 */
static void Isotp_Send_ResponseAddress(void)
{
    if (ctx->tx.en)
    {
        ctx->N_Ar.en = true;
        ctx->N_Ar.last_tick = ctx->tick;

        MicroUds_Trans_ResponseAddress(ctx->tx.data, ctx->tx.len);

        if (ctx->tx_state == ISOTP_TX_SF)
            Isotp_ResetTx();

        ctx->tx.en = false;
        ctx->tx.len = 0u;
        memset(ctx->tx.data, 0, sizeof(ctx->tx.data));
    }

    if (ctx->rx_state == ISOTP_RX_FF)
    {
        switch (ctx->rx_packet.FC.Flow_status)
        {
        case ISOTP_FS_CTS:
            ctx->rx_state = ISOTP_RX_FC;
            ctx->rx_packet.CF.timeout = 0u;
            break;

        case ISOTP_FS_WAIT:
            ctx->rx_state = ISOTP_RX_WAIT;
            ctx->rx_packet.CF.timeout = 0u;
            break;

        default: /* ISOTP_FS_OVER or unknown */
            Isotp_ResetRx();
            break;
        }
    }
}

/* =========================================================================
 * Public API
 * ========================================================================= */

void Isotp_Init(void)
{
    memset(ctx, 0, sizeof(Isotp_Obj_t));
    ctx->rx_packet.buffer = core_obj.rx_buf;
    ctx->tx_packet.buffer = core_obj.tx_buf;
    ctx->rx_state = ISOTP_RX_IDLE;
    ctx->tx_state = ISOTP_TX_IDLE;
}

/**
 * @brief 1ms滴答
 *
 */
void Isotp_TickHandler(void)
{
    ctx->tick++;
}

/**
 * @brief 主循环回调，强烈要求1ms一次
 *
 * @param ReceiveData 接收缓冲区指针，不要传数组，传指针的地址，不要数组，因为大家最后都是指向内核的memory
 * @param ReceiveDataLen 接收到的长度
 * @param ReceiveEn 使能标志，为true时，上层向isotp取数据
 * @param isPhysical 是否是从物理地址收过来的帧 有两个帧，物理地址和功能地址
 */
void Isotp_TimerHandler(uint8_t **ReceiveData, size_t *ReceiveDataLen, bool *ReceiveEn, bool *isPhysical)
{

    if (ctx->frame.en)
        Isotp_ReceiveCallback();

    if (ctx->tx_packet.Isbusy)
        Isotp_TxProcess();

    Isotp_StateCheck();

    /*@brief FC(Wait) 重传逻辑
     *
     * - 接收方可以发送连续的 FC(Wait) 帧。
     * - 每个帧必须在其 N_B 过期之前到达发送方。
     * - 当上层清除 isWaitFs 时，发送一个 FC(CTS)
     * 然后进入 RX_FC。
     */
    if (ctx->rx_state == ISOTP_RX_WAIT)
    {
        if (ctx->rx_packet.FC.isWaitFs)
        {
            if (++ctx->rx_packet.FC.wait_tick >= MICROUDS_FC_WAITTIME) // 作为接收方，每隔MICROUDS_FC_WAITTIMEms 请求一个wait，防止超时
            {
                ctx->rx_packet.FC.wait_tick = 0u;

                if (++ctx->rx_packet.FC.wait_count >= MICROUDS_FC_WAIT_MAX) // 大于自身的发送wait次数，就不管了
                {
                    ctx->tx.en = false;
                    Isotp_ResetRx(); // 本机处理时间太长了，自己静默吧
                }
                else
                {

                    ctx->tx.data[0] = (uint8_t)((ISOTP_TYPE_FC << 4u) | ISOTP_FS_WAIT);
                    ctx->tx.data[1] = (uint8_t)ctx->rx_packet.FC.Bs;
                    ctx->tx.data[2] = (uint8_t)ctx->rx_packet.FC.STmin;
                    memset(&ctx->tx.data[3], MICROUDS_PADDING_BYTES, 5u);
                    ctx->tx.en = true;
                    ctx->tx.len = 8u;
                }
            }
        }
        else
        {
            /*
             * 上层释放了FC(WAIT)。
             * 组装 FC(CTS) 并使用 RX_FF 作为“发送后”哨兵
             * 以便 Isotp_Send_ResponseAddress() 在帧到达总线后转换到 RX_FC
             *
             */
            ctx->rx_packet.FC.Flow_status = ISOTP_FS_CTS;
            ctx->rx_packet.FC.wait_tick = 0u;
            ctx->rx_packet.FC.wait_count = 0u;
            ctx->tx.data[0] = (uint8_t)((ISOTP_TYPE_FC << 4u) | ISOTP_FS_CTS);
            ctx->tx.data[1] = (uint8_t)ctx->rx_packet.FC.Bs;
            ctx->tx.data[2] = (uint8_t)ctx->rx_packet.FC.STmin;
            memset(&ctx->tx.data[3], MICROUDS_PADDING_BYTES, 5u);
            ctx->tx.en = true;
            ctx->tx.len = 8u;
            ctx->rx_state = ISOTP_RX_FF; // 切换状态机
        }
    }
    else
    {
        ctx->rx_packet.FC.wait_tick = 0u;
        ctx->rx_packet.FC.wait_count = 0u;
    }

    // 给上层传递数据
    *isPhysical = ctx->frame.isPhySical;
    *ReceiveEn = false;
    if (ctx->Uplayer.tx.en)
    {
        ctx->Uplayer.tx.en = false;
        *ReceiveData = ctx->rx_packet.buffer;
        *ReceiveDataLen = ctx->Uplayer.tx.len;
        *ReceiveEn = true;
    }

    Isotp_Timer_N_Ar();

    Isotp_Send_ResponseAddress();
}

/**
 * @brief 功能地址接收回调
 *
 * @param data 数据
 * @param len 长度
 */
void Isotp_FunctionAddress(uint8_t *data, size_t len)
{
    if (data == NULL || len == 0u || len > sizeof(ctx->frame.data))
        return;
    memcpy(ctx->frame.data, data, len);
    ctx->frame.len = (uint8_t)len;
    ctx->frame.en = true;
    ctx->frame.isPhySical = false;
}

/**
 * @brief 物理地址回调
 *
 * @param data 数据
 * @param len 长度
 */
void Isotp_PhySicalAddress(uint8_t *data, size_t len)
{
    if (data == NULL || len == 0u || len > sizeof(ctx->frame.data))
        return;
    memcpy(ctx->frame.data, data, len);
    ctx->frame.len = (uint8_t)len;
    ctx->frame.en = true;
    ctx->frame.isPhySical = true;
}

/**
 * @brief 发送数据
 *
 * @param data 数据
 * @param len 长度
 * @return Isotp_Status_t 操作结果
 */
Isotp_Status_t Isotp_Transmit(uint8_t *data, size_t len)
{
    if (data == NULL || len == 0u)
        return ISOTP_ERR;
    if (ctx->tx_packet.Isbusy || ctx->tx_state != ISOTP_TX_IDLE)
        return ISOTP_BUSY;

    if (len <= 7u)
    {
        /* Single Frame */
        // 单帧直接发
        ctx->tx.data[0] = (uint8_t)((ISOTP_TYPE_SF << 4u) | (len & 0x0Fu));
        memcpy(&ctx->tx.data[1], data, len);
        memset(&ctx->tx.data[1 + len], MICROUDS_PADDING_BYTES, 7u - len);
        ctx->tx_packet.Isbusy = true;
        ctx->tx_state = ISOTP_TX_SF;
        ctx->tx.en = true;
        ctx->tx.len = 8u;
    }
    else
    {
        /* Multi-Frame */
        // 多帧走状态机
        if (len > MICROUDS_TX_BUFFER_MEMORY_SIZE)
            return ISOTP_ERR;

        memcpy(ctx->tx_packet.buffer, data, len);
        ctx->tx_packet.total_len = (uint16_t)len;
        ctx->tx_packet.index_len = 6u;
        ctx->tx_packet.SN = 0u;
        ctx->tx_packet.Isbusy = true;

        ctx->tx.data[0] = (uint8_t)((ISOTP_TYPE_FF << 4u) |
                                    ((len >> 8u) & 0x0Fu));
        ctx->tx.data[1] = (uint8_t)(len & 0xFFu);
        memcpy(&ctx->tx.data[2], ctx->tx_packet.buffer, 6u);
        ctx->tx.en = true;
        ctx->tx.len = 8u;
        ctx->tx_state = ISOTP_TX_FF;
    }
    return ISOTP_OK;
}

/**
 * @brief 上层控制流控状态
 * @param  IsWait  true  = wait等待
 *                 false = cts继续
 */
void Isotp_ReceiveFcWait(bool IsWait)
{
    ctx->rx_packet.FC.isWaitFs = IsWait;
}

/**
 * @brief 发送成功的回调，这里主要是让N_Ar停止计时
 *
 */
void Isotp_SendSuccess(void)
{
    ctx->N_Ar.en = false;
}
