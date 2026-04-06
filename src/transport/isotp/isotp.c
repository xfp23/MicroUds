/**
 * @file    isotp.c
 * @author  https://github.com/xfp23
 * @brief   ISO-15765-2 Transport Protocol DOCAN
 * @date    2026/04/02
 */

#include "isotp.h"
#include "string.h"
#include "uds_trans_port.h"
#include "uds_core.h"

static Isotp_Obj_t isotp_obj = {0};
static Isotp_Obj_t *const ctx = &isotp_obj;

/* Reset functions */
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
    memset(ctx->tx_packet.buffer, 0, MICROUDS_TX_BUFFER_MEMORY_SIZE);
    memset(ctx->tx.data, 0, sizeof(ctx->tx.data));
    ctx->tx.en = false;
    ctx->tx.len = 0;
}

static void Isotp_ResetRx(void)
{
    ctx->rx_state = ISOTP_RX_IDLE;
    ctx->rx_packet.CF.timeout = 0;
    ctx->rx_packet.CF.SN = 0;
    ctx->rx_packet.index_len = 0;
    ctx->rx_packet.total_len = 0;
    memset(ctx->rx_packet.buffer, 0, MICROUDS_RX_BUFFER_MEMORY_SIZE);
    /* Reset flow-control wait state */
    ctx->rx_packet.FC.isWaitFs = false;
    ctx->rx_packet.FC.wait_tick = 0;
    ctx->rx_packet.FC.wait_count = 0;
}

/* N_Cr: 接收方等待下一帧 CF 的超时 */
static void Isotp_N_Cr(void)
{
    if (ctx->rx_state == ISOTP_RX_FC)
    {
        if (++ctx->rx_packet.CF.timeout >= MICROUDS_N_CR)
        {
            /* 超时未收到 CF，结束接收 */
            Isotp_ResetRx();
        }
    }
}

/* N_Bs: 发送方等待 FlowControl 帧的超时 */
static void Isotp_N_Bs(void)
{
    if ((ctx->tx_state == ISOTP_TX_FF) ||
        (ctx->tx_state == ISOTP_TX_FC && ctx->tx_packet.FC.Fs == ISOTP_FS_WAIT))
    {
        if (++ctx->tx_packet.N_Bs >= MICROUDS_N_BS)
        {
            /* 超时未收到下一个 FC，终止发送 */
            Isotp_ResetTx();
        }
    }
    else
    {
        ctx->tx_packet.N_Bs = 0;
    }
}

static void Isotp_StateCheck(void)
{
    Isotp_N_Cr();

    /* 如果接收完成，则通知上层 */
    if (ctx->rx_state == ISOTP_RX_CF)
    {
        ctx->Uplayer.tx.en = true;
        ctx->Uplayer.tx.len = ctx->rx_packet.total_len;
        ctx->rx_state = ISOTP_RX_IDLE;
    }
}

/* 接收回调：处理收到的 CAN 数据帧 */
static void Isotp_ReceiveCallback(void)
{
    uint8_t pci = (ctx->frame.data[0] & 0xF0) >> 4;

    /* 处理 FlowControl 帧 (针对发送方) */
    if ((ctx->tx_state == ISOTP_TX_FF || ctx->tx_state == ISOTP_TX_FC) && pci == ISOTP_TYPE_FC)
    {
        uint8_t fs = ctx->frame.data[0] & 0x0F;
        /* 如果是 WAIT */
        if (fs == ISOTP_FS_WAIT)
        {
            if (++ctx->tx_packet.FcWait_count >= MICROUDS_FC_WAIT_MAX)
            {
                /* 超过允许的 WAIT 次数，放弃发送 */
                Isotp_ResetTx();
                return;
            }
        }
        else
        {
            /* CTS 或 OVERFLOW，重置 WAIT 计数 */
            ctx->tx_packet.FcWait_count = 0;
        }
        ctx->tx_packet.FC.Fs = fs;
        ctx->tx_packet.FC.Bs = ctx->frame.data[1];
        ctx->tx_packet.FC.STmin = ctx->frame.data[2];
        ctx->tx_packet.N_Bs = 0; /* 收到 FC，重置 N_BS 计时 */

        if (fs == ISOTP_FS_CTS)
        {
            /* 对方允许继续发送 */
            ctx->tx_packet.cf_count = 0;
            ctx->tx_state = ISOTP_TX_CF;
        }
        else if (fs == ISOTP_FS_OVER)
        {
            /* 对方缓冲不足，终止发送 */
            Isotp_ResetTx();
        }
        /* fs == WAIT 时保持 TX_FC 状态继续等待 */
        return;
    }

    /* 处理非 FlowControl 的帧 */
    switch (pci)
    {
        case ISOTP_TYPE_SF:
        {
            /* 单帧 */
            uint8_t sf_len = ctx->frame.data[0] & 0x0F;
            /* 验证长度 */
            if (sf_len == 0 || sf_len > 7 || sf_len > (ctx->frame.len - 1))
            {
                Isotp_ResetRx();
                break;
            }
            memset(ctx->rx_packet.buffer, 0, MICROUDS_RX_BUFFER_MEMORY_SIZE);
            memcpy(ctx->rx_packet.buffer, &ctx->frame.data[1], sf_len);
            ctx->Uplayer.tx.en = true;
            ctx->Uplayer.tx.len = sf_len;
            break;
        }
        case ISOTP_TYPE_FF:
        {
            /* 首帧 */
            if (!ctx->frame.isPhySical)
            {
                /* 功能寻址不处理多帧 */
                return;
            }
            if (ctx->rx_state != ISOTP_RX_IDLE)
            {
                /* 正在接收其他消息，丢弃 */
                return;
            }
            ctx->rx_state = ISOTP_RX_FF;
            ctx->rx_packet.total_len = ((uint16_t)(ctx->frame.data[0] & 0x0F) << 8)
                                        | ctx->frame.data[1];
            ctx->rx_packet.index_len = 6;
            memset(ctx->rx_packet.buffer, 0, MICROUDS_RX_BUFFER_MEMORY_SIZE);
            memcpy(ctx->rx_packet.buffer, &ctx->frame.data[2], 6);

            /* 判断是否溢出 */
            if (ctx->rx_packet.total_len > MICROUDS_RX_BUFFER_MEMORY_SIZE)
            {
                ctx->rx_packet.FC.Flow_status = ISOTP_FS_OVER;
            }
            else
            {
                ctx->rx_packet.FC.Flow_status = ISOTP_FS_CTS;
            }

            ctx->rx_packet.FC.Bs = MICROUDS_BLOCK_SIZE;
            ctx->rx_packet.FC.STmin = MICROUDS_SEPARATION_TIME;

            /* 如果上层请求等待，则覆盖为 WAIT */
            if (ctx->rx_packet.FC.isWaitFs)
            {
                ctx->rx_packet.FC.Flow_status = ISOTP_FS_WAIT;
            }

            /* 发送流控帧 */
            ctx->tx.data[0] = (ISOTP_TYPE_FC << 4) | ctx->rx_packet.FC.Flow_status;
            ctx->tx.data[1] = (uint8_t)ctx->rx_packet.FC.Bs;
            ctx->tx.data[2] = (uint8_t)ctx->rx_packet.FC.STmin;
            memset(ctx->tx.data + 3, MICROUDS_PADDING_BYTES, 5);
            ctx->tx.en = true;
            ctx->tx.len = 8;
            ctx->rx_packet.CF.SN = 0;

            /* 设置后续状态 */
            if (ctx->rx_packet.FC.Flow_status == ISOTP_FS_CTS)
            {
                ctx->rx_state = ISOTP_RX_FC;
                ctx->rx_packet.CF.timeout = 0;
            }
            else if (ctx->rx_packet.FC.Flow_status == ISOTP_FS_WAIT)
            {
                ctx->rx_state = ISOTP_RX_WAIT;
                ctx->rx_packet.CF.timeout = 0;
                ctx->rx_packet.FC.wait_tick = 0;
                ctx->rx_packet.FC.wait_count = 0;
            }
            break;
        }
        case ISOTP_TYPE_CF:
        {
            /* 连续帧 */
            if (!ctx->frame.isPhySical)
            {
                /* 功能寻址不接收多帧 */
                return;
            }
            if (ctx->rx_state != ISOTP_RX_FC)
            {
                return;
            }
            ctx->rx_packet.CF.timeout = 0;
            uint8_t sn = ctx->frame.data[0] & 0x0F;
            uint8_t expected = (ctx->rx_packet.CF.SN + 1) & 0x0F;
            if (sn != expected)
            {
                /* 序号错误 */
                Isotp_ResetRx();
                return;
            }
            ctx->rx_packet.CF.SN = sn;
            uint16_t remain = ctx->rx_packet.total_len - ctx->rx_packet.index_len;
            uint8_t copy_len = (remain > 7) ? 7 : (uint8_t)remain;
            memcpy(ctx->rx_packet.buffer + ctx->rx_packet.index_len, &ctx->frame.data[1], copy_len);
            ctx->rx_packet.index_len += copy_len;
            if (ctx->rx_packet.index_len >= ctx->rx_packet.total_len)
            {
                /* 完成接收 */
                ctx->rx_state = ISOTP_RX_CF;
            }
            break;
        }
        default:
            break;
    }

    /* 清除帧缓存 */
    memset(ctx->frame.data, 0, sizeof(ctx->frame.data));
    ctx->frame.en = false;
    ctx->frame.len = 0;
}

/* 定期调用，处理发送过程 */
static void Isotp_UpLayerReceive(void)
{
    switch (ctx->tx_state)
    {
        case ISOTP_TX_FF:
            /* 发送了首帧，等待流控 */
            Isotp_N_Bs();
            break;
        case ISOTP_TX_FC:
            /* 正在等待流控，定时器已处理 */
            Isotp_N_Bs();
            break;
        case ISOTP_TX_CF:
        {
            /* 连续帧发送 */
            if ((ctx->tick - ctx->tx_packet.last_tick) < ctx->tx_packet.FC.STmin)
            {
                return;
            }
            ctx->tx_packet.last_tick = ctx->tick;
            if (ctx->tx_packet.index_len >= ctx->tx_packet.total_len)
            {
                /* 全部发送完成 */
                Isotp_ResetTx();
                return;
            }
            /* 填充下一个 CF */
            if (++ctx->tx_packet.SN > 0x0F)
            {
                ctx->tx_packet.SN = 0;
            }
            ctx->tx.data[0] = (ISOTP_TYPE_CF << 4) | (ctx->tx_packet.SN & 0x0F);
            uint16_t remain = ctx->tx_packet.total_len - ctx->tx_packet.index_len;
            uint8_t payload_len = (remain > 7) ? 7 : (uint8_t)remain;
            memset(ctx->tx.data + 1, MICROUDS_PADDING_BYTES, 7);
            memcpy(&ctx->tx.data[1], &ctx->tx_packet.buffer[ctx->tx_packet.index_len], payload_len);
            ctx->tx_packet.index_len += payload_len;
            if (ctx->tx_packet.FC.Bs != 0)
            {
                if (++ctx->tx_packet.cf_count >= ctx->tx_packet.FC.Bs)
                {
                    /* 达到块大小，等待下一个 FC */
                    ctx->tx_state = ISOTP_TX_FF;
                    ctx->tx_packet.N_Bs = 0;
                    ctx->tx_packet.cf_count = 0;
                }
            }
            ctx->tx.en = true;
            ctx->tx.len = 8;
            break;
        }
        default:
            break;
    }
}

/* 发送队列中的帧 */
static void Isotp_Send_ResponseAddress(void)
{
    if (ctx->tx.en)
    {
        MicroUds_Trans_ResponseAddress(ctx->tx.data, ctx->tx.len);

        if (ctx->tx_state == ISOTP_TX_SF)
        {
            Isotp_ResetTx();
        }

        ctx->tx.en = false;
        ctx->tx.len = 0;
        memset(ctx->tx.data, 0, sizeof(ctx->tx.data));
    }

    /* 发送流控后的状态跳转 */
    if (ctx->rx_state == ISOTP_RX_FF)
    {
        if (ctx->rx_packet.FC.Flow_status == ISOTP_FS_CTS)
        {
            ctx->rx_state = ISOTP_RX_FC;
            ctx->rx_packet.CF.timeout = 0;
        }
        else if (ctx->rx_packet.FC.Flow_status == ISOTP_FS_WAIT)
        {
            ctx->rx_state = ISOTP_RX_WAIT;
            ctx->rx_packet.CF.timeout = 0;
        }
        else
        {
            Isotp_ResetRx();
        }
    }
}

/* 初始化 */
void Isotp_Init(void)
{
    memset((void*)ctx, 0, sizeof(Isotp_Obj_t));
    ctx->rx_packet.buffer = core_obj.rx_buf;
    ctx->tx_packet.buffer = core_obj.tx_buf;
    ctx->tick = 0;
    ctx->rx_state = ISOTP_RX_IDLE;
    ctx->tx_state = ISOTP_TX_IDLE;
}

/* 每毫秒调用一次 */
void Isotp_Tick(void)
{
    ctx->tick++;
}

void Isotp_TimerHandler(uint8_t **ReceiveData, size_t *ReceiveDataLen, bool *ReceiveEn, bool *isPhysical)
{
    if (ctx->frame.en)
    {
        Isotp_ReceiveCallback();
    }
    if (ctx->tx_packet.Isbusy)
    {
        Isotp_UpLayerReceive();
    }
    Isotp_StateCheck();

    /* 重发 WAIT (如果有请求) */
    if (ctx->rx_packet.FC.isWaitFs && ctx->rx_state == ISOTP_RX_WAIT)
    {
        if (++ctx->rx_packet.FC.wait_tick >= MICROUDS_FC_WAITTIME)
        {
            ctx->rx_packet.FC.wait_tick = 0;
            if (++ctx->rx_packet.FC.wait_count >= MICROUDS_FC_WAIT_MAX)
            {
                /* WAIT 超限，放弃接收 */
                Isotp_ResetRx();
                *ReceiveEn = false;
                return;
            }
            /* 重新发送 WAIT */
            ctx->tx.data[0] = (ISOTP_TYPE_FC << 4) | ISOTP_FS_WAIT;
            ctx->tx.data[1] = (uint8_t)ctx->rx_packet.FC.Bs;
            ctx->tx.data[2] = (uint8_t)ctx->rx_packet.FC.STmin;
            memset(ctx->tx.data + 3, MICROUDS_PADDING_BYTES, 5);
            ctx->tx.en = true;
            ctx->tx.len = 8;
        }
    }
    else
    {
        ctx->rx_packet.FC.wait_tick = 0;
        ctx->rx_packet.FC.wait_count = 0;
    }

    *isPhysical = ctx->frame.isPhySical;
    *ReceiveEn = false;
    if (ctx->Uplayer.tx.en)
    {
        ctx->Uplayer.tx.en = false;
        *ReceiveData = ctx->rx_packet.buffer;
        *ReceiveDataLen = ctx->Uplayer.tx.len;
        *ReceiveEn = true;
    }

    Isotp_Send_ResponseAddress();
}

/* 接收原始帧 (功能寻址) */
void Isotp_FunctionAddress(uint8_t *data, size_t len)
{
    memcpy(ctx->frame.data, data, len);
    ctx->frame.len = (uint8_t)len;
    ctx->frame.en = true;
    ctx->frame.isPhySical = false;
}

/* 接收原始帧 (物理寻址) */
void Isotp_PhySicalAddress(uint8_t *data, size_t len)
{
    memcpy(ctx->frame.data, data, len);
    ctx->frame.len = (uint8_t)len;
    ctx->frame.en = true;
    ctx->frame.isPhySical = true;
}

/* 上层调用发送 */
Isotp_Status_t Isotp_Transmit(uint8_t *data, size_t len)
{
    if (data == NULL || len == 0)
    {
        return ISOTP_ERR;
    }
    if (ctx->tx_packet.Isbusy || ctx->tx_state != ISOTP_TX_IDLE)
    {
        return ISOTP_BUSY;
    }
    if (len <= 7)
    {
        /* 单帧发送 */
        ctx->tx.data[0] = (ISOTP_TYPE_SF << 4) | (len & 0x0F);
        memcpy(&ctx->tx.data[1], data, len);
        ctx->tx_packet.Isbusy = true;
        ctx->tx_state = ISOTP_TX_SF;
        ctx->tx.en = true;
        ctx->tx.len = 8;
        return ISOTP_OK;
    }
    /* 多帧发送 */
    ctx->tx_state = ISOTP_TX_FF;
    ctx->tx_packet.Isbusy = true;
    memcpy(ctx->tx_packet.buffer, data, len);
    ctx->tx_packet.total_len = (uint16_t)len;
    ctx->tx_packet.index_len = 6;
    ctx->tx_packet.SN = 0;
    ctx->tx.data[0] = (uint8_t)((ISOTP_TYPE_FF << 4) | ((len >> 8) & 0x0F));
    ctx->tx.data[1] = (uint8_t)(len & 0xFF);
    memcpy(&ctx->tx.data[2], ctx->tx_packet.buffer, 6);
    ctx->tx.en = true;
    ctx->tx.len = 8;
    return ISOTP_OK;
}

/* 上层请求发送 WAIT 流控 */
void Isotp_ReceiveFcWait(bool IsWait)
{
    ctx->rx_packet.FC.isWaitFs = IsWait;
}
