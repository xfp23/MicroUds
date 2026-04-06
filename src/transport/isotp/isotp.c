/**
 * @file    isotp.c
 * @author  https://github.com/xfp23
 * @version 0.1
 * @date    2026/04/02
 */

/* Includes ------------------------------------------------------------------*/
#include "isotp.h"
#include "string.h"
#include "uds_trans_port.h"
#include "uds_core.h"

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/
// #define TX_ENABLE() this->
/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
static Isotp_Obj_t isotp_obj = {0};
static Isotp_Obj_t *const this = &isotp_obj;
/* Private function prototypes -----------------------------------------------*/

// 接收方收到上一帧 CF 后，等待下一帧 CF 达到的时间。
static void Isotp_N_Cr()
{                                        // 这个判断不能要，如果我发送等待的次数太多导致对方静默了，那我自己就死锁了
    if (this->rx_state != ISOTP_RX_FC /*|| this->rx_packet.FC.Flow_status != ISOTP_FS_CTS */) // 发完流控就要等CF超时时间了
    {
        return;
    }

    if (++this->rx_packet.CF.timeout >= MICROUDS_N_CR)
    {
        this->rx_packet.CF.timeout = 0;
        this->rx_state = ISOTP_RX_IDLE;
        memset(this->rx_packet.buffer, 0, MICROUDS_RX_BUFFER_MEMORY_SIZE); // 清空内存
    }
}

static void Isotp_ResetTx(void)
{
    this->tx_state = ISOTP_TX_IDLE;
    this->tx_packet.Isbusy = false;
    this->tx_packet.N_Bs = 0;
    this->tx_packet.FcWait_count = 0;
    this->tx_packet.cf_count = 0;
    this->tx_packet.index_len = 0;
    this->tx_packet.total_len = 0;
    memset(this->tx_packet.buffer, 0, MICROUDS_TX_BUFFER_MEMORY_SIZE);
    memset(this->tx.data, 0, sizeof(this->tx.data));
    this->tx.en = false;
    this->tx.len = 0;
}

static void Isotp_ResetRx(void)
{
    this->rx_state = ISOTP_RX_IDLE;
    this->rx_packet.CF.timeout = 0;
    this->rx_packet.CF.SN = 0;
    this->rx_packet.index_len = 0;
    this->rx_packet.total_len = 0;
    memset(this->rx_packet.buffer, 0, MICROUDS_RX_BUFFER_MEMORY_SIZE);
}

static void Isotp_StateCheck()
{
    Isotp_N_Cr();

    if (this->rx_state == ISOTP_RX_CF) // 就是说收完了，发给上层
    {
        this->Uplayer.tx.en = true;
        this->Uplayer.tx.len = this->rx_packet.total_len;
        this->rx_state = ISOTP_RX_IDLE;
    }
}
// 发送方发出 FF 后，等待接收方回 FC 的时间。
static void Isotp_N_Bs()
{
    if (this->tx_state == ISOTP_TX_FF)
    {
        if (++this->tx_packet.N_Bs >= MICROUDS_N_BS)
        {
            this->tx_packet.N_Bs = 0;
            this->tx_state = ISOTP_TX_IDLE; // N_Bs空闲
            this->tx_packet.Isbusy = false;
            Isotp_ResetTx();
        }
    }
    else if (this->tx_state == ISOTP_TX_FC && this->tx_packet.FC.Fs == ISOTP_FS_WAIT)
    {
        // 等待流控
        if (++this->tx_packet.N_Bs >= MICROUDS_N_BS)
        {
            this->tx_packet.N_Bs = 0;
            this->tx_state = ISOTP_TX_IDLE; // 空闲
            this->tx_packet.Isbusy = false;
        }
    }
    else
    {
        this->tx_packet.N_Bs = 0;
        this->tx_packet.FcWait_count = 0;
    }
}
static void Isotp_ReceiveCallback()
{
    // uint8_t pci = 0x00;
    this->frame.pci = (this->frame.data[0] & 0xF0) >> 4; // 取得高四位

    // if ((this->tx_state == ISOTP_TX_FF || this->tx_state == ISOTP_TX_FC) && this->frame.pci == ISOTP_TYPE_FC) // 如果是发送首帧的话，就不必走下面接收状态机了吧.
    // {
    //     if (++this->tx_packet.FcWait_count >= MICROUDS_FC_WAIT_MAX)
    //     {
    //         this->tx_packet.FcWait_count = 0;
    //         this->tx_state = ISOTP_TX_IDLE; // 空闲
    //     }

    //     this->tx_packet.FC.Fs = this->frame.data[0] & 0x0F;
    //     this->tx_packet.FC.Bs = this->frame.data[1];
    //     this->tx_packet.FC.STmin = this->frame.data[2];
    //     this->tx_state = ISOTP_TX_FC;
    //     return;
    // }
    // else
    // {
    //     this->tx_packet.FcWait_count = 0;
    // }

    if ((this->tx_state == ISOTP_TX_FF || this->tx_state == ISOTP_TX_FC) && this->frame.pci == ISOTP_TYPE_FC)
    {
        uint8_t fs = this->frame.data[0] & 0x0F;

        if (fs == ISOTP_FS_WAIT)
        {
            if (++this->tx_packet.FcWait_count >= MICROUDS_FC_WAIT_MAX)
            {
                Isotp_ResetTx(); // 超过等待次数，放弃
                return;
            }
        }
        else
        {
            this->tx_packet.FcWait_count = 0; // CTS/OVER 时清零
        }

        this->tx_packet.FC.Fs = fs;
        this->tx_packet.FC.Bs = this->frame.data[1];
        this->tx_packet.FC.STmin = this->frame.data[2];
        this->tx_packet.N_Bs = 0; // 收到FC，N_Bs计时器复位
        this->tx_state = ISOTP_TX_FC;
        return;
    }
    switch (this->frame.pci)
    {

    case ISOTP_TYPE_SF: // 单帧
        this->Uplayer.tx.en = true;
        this->Uplayer.tx.len = this->frame.data[0] & 0x0F;
        memset(this->rx_packet.buffer, 0, 8); // 单帧最多才7字节，清空前面8个就够了。不需要每次都清理全部
        memcpy((void *)this->rx_packet.buffer, (void *)&this->frame.data[1], this->frame.len - 1);
        break;
    case ISOTP_TYPE_FF: // 首帧
        if (!this->frame.isPhySical)
        {
            // Isotp_ResetRx();
            return;
        }
        if (this->rx_state != ISOTP_RX_IDLE)
        {
            return;
        }
        this->rx_state = ISOTP_RX_FF;
        // this->rx_state = ISOTP_RX_FC;  // 发流控
        this->rx_packet.total_len = ((uint16_t)(this->frame.data[0] & 0x0F) << 8) | (this->frame.data[1]); // 获取总长度,共12位
        this->rx_packet.index_len = 6;
        memset((void *)this->rx_packet.buffer, 0, MICROUDS_RX_BUFFER_MEMORY_SIZE); // 清空接收buffer
        memcpy(this->rx_packet.buffer, &this->frame.data[2], 6);

        if (this->rx_packet.total_len > MICROUDS_RX_BUFFER_MEMORY_SIZE)
        {
            this->rx_packet.FC.Flow_status = ISOTP_FS_OVER;
        }
        else
        {
            this->rx_packet.FC.Flow_status = ISOTP_FS_CTS;
        }

        this->rx_packet.FC.STmin = MICROUDS_SEPARATION_TIME;
        this->rx_packet.FC.Bs = MICROUDS_BLOCK_SIZE;

        if (this->rx_packet.FC.isWaitFs == true)
        {
            this->rx_packet.FC.Flow_status = ISOTP_FS_WAIT;
        }
        this->tx.data[0] = (ISOTP_TYPE_FC << 4) | this->rx_packet.FC.Flow_status;
        this->tx.data[1] = this->rx_packet.FC.Bs;
        this->tx.data[2] = this->rx_packet.FC.STmin;
        memset(this->rx_packet.FC.Fc_payload, MICROUDS_PADDING_BYTES, sizeof(this->rx_packet.FC.Fc_payload));
        memcpy((void *)&this->tx.data[3], (void *)this->rx_packet.FC.Fc_payload, sizeof(this->rx_packet.FC.Fc_payload));
        this->tx.en = true;
        this->tx.len = 8;
        this->rx_packet.CF.SN = 0;
        break;

    case ISOTP_TYPE_CF: // 连续帧
        if (!this->frame.isPhySical)
        {
            // Isotp_ResetRx();
            return;
        }
        // static uint8_t isCFing = false;
        if (this->rx_state != ISOTP_RX_FC)
        {
            return;
        }

        this->rx_packet.CF.timeout = 0;
        uint8_t sn = this->frame.data[0] & 0x0F;
        if (sn != ++this->rx_packet.CF.SN)
        {
            // 数据包错乱，直接丢弃整个buffer数据，结束本次传输，进入静默状态，等待N_BS超时
            memset(this->rx_packet.buffer, 0, MICROUDS_RX_BUFFER_MEMORY_SIZE);
            this->rx_state = ISOTP_RX_IDLE; // 回到空闲
            return;
        }

        if (this->rx_packet.CF.SN >= 15)
        {
            this->rx_packet.CF.SN = 0;
        }

        uint16_t copy_len = this->rx_packet.total_len - (this->rx_packet.index_len);
        if (copy_len > 7)
            copy_len = 7;
        memcpy(this->rx_packet.buffer + this->rx_packet.index_len, &this->frame.data[1], copy_len);
        this->rx_packet.index_len += copy_len;
        if (this->rx_packet.index_len >= this->rx_packet.total_len)
        {
            this->rx_state = ISOTP_RX_CF; // 收完cf
        }

        break;

    default:
        break;
    }
    // CLEAR: // clear
    memset(this->frame.data, 0, sizeof(this->frame.data));
    this->frame.en = false;
    this->frame.len = 0;
}

static void Isotp_UpLayerReceive() // 从上层接收到数据对外发送
{

    // if(this->tx_state == ISOTP_TX_FF)
    // {
    //     // 等待N_BS
    //     Isotp_N_Bs();
    // }

    switch (this->tx_state)
    {
    case ISOTP_TX_FF:
        /* code */
        Isotp_N_Bs();
        break;
    case ISOTP_TX_FC:
        if (this->tx_packet.FC.Fs == ISOTP_FS_CTS)
        {
            this->tx_packet.cf_count = 0;
            this->tx_state = ISOTP_TX_CF;
        }
        else if (this->tx_packet.FC.Fs == ISOTP_FS_OVER)
        {
            this->tx_state = ISOTP_TX_IDLE; // 放弃发送
            memset(this->tx_packet.buffer, 0, MICROUDS_TX_BUFFER_MEMORY_SIZE);
        }
        else if (this->tx_packet.FC.Fs == ISOTP_FS_WAIT)
        {
            // 等待N_BS
            Isotp_N_Bs();
        }
        // else if (this->tx_packet.FC.Fs == ISOTP_FS_CTS)
        // {
        //     // this->tx_packet.cf_count = 0;
        //     this->tx_state = ISOTP_TX_CF;
        // }
        break;

    case ISOTP_TX_CF:

        if (this->tick - this->tx_packet.last_tick < this->tx_packet.FC.STmin)
        {
            return; // 流控时间没到
        }

        this->tx_packet.last_tick = this->tick;
        if (this->tx_packet.index_len >= this->tx_packet.total_len)
        {
            // 发完了
            this->tx_state = ISOTP_TX_IDLE;
            this->tx_packet.index_len = 0;
            this->tx_packet.Isbusy = false;
            return;
        }

        // 根据流控发数据

        if (++this->tx_packet.SN > 0x0F)
        {
            this->tx_packet.SN = 0x00; // 回绕，下次发送会变成 0x01
        }
        this->tx.data[0] = (ISOTP_TYPE_CF << 4) | this->tx_packet.SN;
        memcpy((void *)&this->tx.data[1], (void *)&this->tx_packet.buffer[this->tx_packet.index_len], 7);
        this->tx_packet.index_len += 7;

        if (this->tx_packet.FC.Bs != 0 && ++this->tx_packet.cf_count >= this->tx_packet.FC.Bs)
        {
            this->tx_state = ISOTP_TX_FF; // 继续回到首帧被发完的状态,继续等待流控
            this->tx_packet.N_Bs = 0;
        }
        this->tx.en = true;
        this->tx.len = 8;

        break;

    case ISOTP_TX_IDLE:
        this->tx_packet.last_tick = this->tick; // 同步节拍
        this->tx_packet.SN = 0;
        break;
    default:
        break;
    }
}

static void Isotp_Send_ResponseAddress()
{
    if (this->tx.en)
    {
        MicroUds_Trans_ResponseAddress(this->tx.data, this->tx.len);

        if (this->tx_state == ISOTP_TX_SF)
        {
            this->tx_packet.Isbusy = false; // 只是单帧发送的话可以直接清空
            this->tx_state = ISOTP_TX_IDLE; // 空闲
        }

        // if (this->rx_state == ISOTP_RX_FF)
        // {
        //     this->rx_state = ISOTP_RX_FC;
        // }

        if (this->rx_state == ISOTP_RX_FF)
        {
            if (this->rx_packet.FC.Flow_status == ISOTP_FS_CTS)
            {
                this->rx_state = ISOTP_RX_FC;
                this->rx_packet.CF.timeout = 0;
            }
            // else if (this->rx_packet.FC.Flow_status == ISOTP_FS_WAIT)
            // {
            //     this->rx_state = ISOTP_RX_IDLE;
            //     this->rx_packet.CF.timeout = 0;
            // }
            else if (this->rx_packet.FC.Flow_status == ISOTP_FS_WAIT)
            {
                this->rx_state = ISOTP_RX_FC; // 保持FC状态，继续等CF或超时
                this->rx_packet.CF.timeout = 0;
            }
            else
            {
                this->rx_state = ISOTP_RX_IDLE;
            }
        }

        this->tx.en = false;
        this->tx.len = 0;
        memset(this->tx.data, 0, sizeof(this->tx.data));
    }
}

/* Exported functions --------------------------------------------------------*/

/* Private functions ---------------------------------------------------------*/

void Isotp_Init(void)
{
    memset((void *)this, 0, sizeof(Isotp_Obj_t));
    this->rx_packet.buffer = core_obj.rx_buf; // 接收缓冲区初始化
    this->tx_packet.buffer = core_obj.tx_buf; // 发送缓冲区初始化
}

void Isotp_Tick(void)
{
    this->tick++;
}

/**
 * @brief
 *
 * @param ReceiveData 接收数据的指针地址，不需要再来一个数组
 * @param ReceiveDataLen 接收到的数据长度
 * @param ReceiveEn 只有为true才表示有数据进来
 * @param isPhysical 区分物理地址和响应地址
 */
void Isotp_TimerHandler(uint8_t **ReceiveData, size_t *ReceiveDataLen, bool *ReceiveEn, bool *isPhysical)
{
    if (this->frame.en)
    {
        Isotp_ReceiveCallback();
    }

    // if(this->Uplayer.rx.en)

    if (this->tx_packet.Isbusy)
    {
        Isotp_UpLayerReceive();
    }
    Isotp_StateCheck();

    // if(this->rx_packet.FC.isWaitFs && this->rx_state == ISOTP_RX_FF && ++this->rx_packet.FC.wait_tick >= MICROUDS_FC_WAITTIME)
    // {
    //     this->rx_packet.FC.wait_tick = 0;
    //     this->tx.en = true;
    //     this->tx.len = 8;
    //     this->tx.data[0] = ISOTP_TYPE_FC << 4 | ISOTP_FS_WAIT; 
    //     this->tx.data[1] = this->rx_packet.FC.Bs;
    //     this->tx.data[2] = this->rx_packet.FC.STmin;
        
    // } else {
    //     this->rx_packet.FC.wait_tick = 0;
    //     this->rx_packet.FC.wait_count = 0;
    // }

    // 条件改为 ISOTP_RX_FC，并加次数限制
if (this->rx_packet.FC.isWaitFs
    && this->rx_state == ISOTP_RX_FC
    && ++this->rx_packet.FC.wait_tick >= MICROUDS_FC_WAITTIME)
{
    this->rx_packet.FC.wait_tick = 0;

    if (++this->rx_packet.FC.wait_count >= MICROUDS_FC_WAIT_MAX)
    {
        // 超过最大等待次数，放弃本次接收
        Isotp_ResetRx();
        return;
    }

    // 重发 WAIT FC
    this->tx.data[0] = (ISOTP_TYPE_FC << 4) | ISOTP_FS_WAIT;
    this->tx.data[1] = this->rx_packet.FC.Bs;
    this->tx.data[2] = this->rx_packet.FC.STmin;
    this->tx.en  = true;
    this->tx.len = 8;
}
else if (!this->rx_packet.FC.isWaitFs)
{
    this->rx_packet.FC.wait_tick  = 0;
    this->rx_packet.FC.wait_count = 0;
}
    *isPhysical = this->frame.isPhySical;
    *ReceiveEn = false;
    if (this->Uplayer.tx.en) // 给上层传输数据
    {
        this->Uplayer.tx.en = false;
        *ReceiveData = this->rx_packet.buffer; // 不拷贝，直接返回系统全局buffer地址
        *ReceiveDataLen = this->Uplayer.tx.len;
        *ReceiveEn = true; // 只有在此值为true才允许操作 ReceiveData
    }

    Isotp_Send_ResponseAddress(); // 发送动作不归我管，定时调用即可
}

void Isotp_FunctionAddress(uint8_t *data, size_t len)
{
    memcpy((void *)this->frame.data, data, len);
    this->frame.en = true;
    this->frame.isPhySical = false;

    // Isotp_ReceiveCallback();
}

void Isotp_PhySicalAddress(uint8_t *data, size_t len) // 物理地址
{
    memcpy((void *)this->frame.data, data, len);
    this->frame.len = len;
    this->frame.en = true;
    this->frame.isPhySical = true;

    // Isotp_ReceiveCallback();
}

/**
 * @brief 由上层调用发送
 *
 * @param data 数据
 * @param len 长度
 */
Isotp_Status_t Isotp_Transmit(uint8_t *data, size_t len)
{
    if (data == NULL || len <= 0)
    {
        return ISOTP_ERR;
    }

    if (this->tx_packet.Isbusy || this->tx_state != ISOTP_TX_IDLE) // 好像引入状态机之后这个isbusy可以取消掉了
    {
        return ISOTP_BUSY;
    }

    if (len <= 7)
    {
        this->tx.data[0] = (ISOTP_TYPE_SF << 4 | len);
        memcpy((void *)&this->tx.data[1], (void *)data, len);
        this->tx_packet.Isbusy = true;
        this->tx.en = true;
        this->tx.len = 8;
        this->tx_state = ISOTP_TX_SF;

        return ISOTP_OK;
    }

    this->tx_state = ISOTP_TX_FF;
    memcpy((void *)this->tx_packet.buffer, data, len); // 把数据拷贝到缓冲区
    this->tx_packet.total_len = len;
    this->tx_packet.index_len = 0;
    this->tx_packet.Isbusy = true;
    this->tx_packet.index_len = 6;
    this->tx.data[0] = (uint8_t)((ISOTP_TYPE_FF << 4) | ((this->tx_packet.total_len >> 8) & 0x0F)); // 取 bit11~8
    this->tx.data[1] = (uint8_t)(this->tx_packet.total_len & 0x00FF);
    memcpy((void *)&this->tx.data[2], this->tx_packet.buffer, 6);
    this->tx.en = true;
    this->tx.len = 8;

    return ISOTP_OK;
}

/**
 * @brief 作为接收方的时候，由上层决定是否发送wait等待
 *
 * @param IsWait 流控发送等待 ： true  正常继续 ： false  | 溢出操作归库自己处理
 */
void Isotp_ReceiveFcWait(bool IsWait)
{
    this->rx_packet.FC.isWaitFs = IsWait;
}
