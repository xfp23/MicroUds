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
{
    if(this->rx_state != ISOTP_RX_FF)
    {
        return;
    }

    if(this->rx_packet.CF.timeout++ >= MICROUDS_N_CR)
    {
        this->rx_state = ISOTP_RX_IDLE;
        memset(this->rx_packet.buffer,0,MICROUDS_RX_BUFFER_MEMORY_SIZE); // 清空内存
    }
}

static void Isotp_StateCheck()
{
    Isotp_N_Cr();

    if(this->rx_state == ISOTP_RX_CF)
    {
        this->Uplayer.tx.en = true;
        this->Uplayer.tx.len = this->rx_packet.total_len;
        this->rx_state = ISOTP_RX_IDLE;
    }


}
// 发送方发出 FF 后，等待接收方回 FC 的时间。
static void Isotp_N_Bs()
{

}
static void Isotp_ReceiveCallback()
{
    // uint8_t pci = 0x00;
    this->frame.pci = this->frame.data[0] & 0xF0; // 取得高四位

    switch (this->frame.pci)
    {
    
    case ISOTP_TYPE_SF: // 单帧
        this->Uplayer.tx.en = true;
        this->Uplayer.tx.len = this->frame.data[0] & 0x0F;
        memset(this->rx_packet.buffer,0,8); // 单帧最多才7字节，清空前面8个就够了。不需要每次都清理全部
        memcpy((void*)this->rx_packet.buffer,(void*)&this->frame.data[1],this->frame.len -1);
        break;
    case ISOTP_TYPE_FF: // 首帧
        if(this->rx_state != ISOTP_RX_IDLE)
        {
            return;
        }
        this->rx_state = ISOTP_RX_FF;
        this->rx_packet.total_len = ((this->frame.data[0] & 0x0F) << 4) | (this->frame.data[1]); // 获取总长度,共12位
        this->rx_packet.index_len = 6;
        memset((void*)this->rx_packet.buffer,0,MICROUDS_RX_BUFFER_MEMORY_SIZE);
        memcpy((void*)this->rx_packet.buffer,this->frame.data,6);

        if(this->rx_packet.total_len >= MICROUDS_RX_BUFFER_MEMORY_SIZE)
        {
            this->rx_packet.FC.Flow_status = ISOTP_FS_OVER;
        }

            this->rx_packet.FC.STmin = MICROUDS_SEPARATION_TIME;
            this->rx_packet.FC.Bs = MICROUDS_BLOCK_SIZE;
            this->rx_packet.FC.Flow_status = ISOTP_FS_CON;
            if(this->rx_packet.FC.isWaitFs == true)
            {
                this->rx_packet.FC.Flow_status = ISOTP_FS_WAIT;
            }
            this->tx.data[0] = (ISOTP_TYPE_FC << 4) | this->rx_packet.FC.Flow_status;
            this->tx.data[1] = this->rx_packet.FC.Bs;
            this->tx.data[2] = this->rx_packet.FC.STmin;
            memset(this->rx_packet.FC.Fc_payload,MICROUDS_PADDING_BYTES,sizeof(this->rx_packet.FC.Fc_payload));
            memcpy((void*)&this->tx.data[3],(void*)this->rx_packet.FC.Fc_payload,sizeof(this->rx_packet.FC.Fc_payload));
            this->tx.en = true;
            this->tx.len = 8;
            this->rx_packet.CF.SN = 0;
        break;
    
    case ISOTP_TYPE_CF: // 连续帧
            // static uint8_t isCFing = false;
        if (this->rx_state != ISOTP_RX_FF)
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

            if(this->rx_packet.CF.SN >= 15)
            {
                this->rx_packet.CF.SN = 0;
            }

            this->rx_packet.index_len += 7;
            if(this->rx_packet.index_len >= this->rx_packet.total_len)
            {
                this->rx_state = ISOTP_RX_CF; // 收完cf
            }
        
        break;

    default:
        break;
    }
CLEAR: // clear
    memset(this->frame.data,0,sizeof(this->frame.data));
    this->frame.en = false;
    this->frame.len = 0;
}

static void Isotp_UpLayerReceive()
{

}

static void Isotp_Send_ResponseAddress()
{
    if(this->tx.en)
    {
        MicroUds_Trans_ResponseAddress(this->tx.data,this->tx.len);

        this->tx_packet.Isbusy = false;
        this->tx.en = false;
        this->tx.len = 0;
        memset(this->tx.data,0,sizeof(this->tx.data));
    }
}

/* Exported functions --------------------------------------------------------*/

/* Private functions ---------------------------------------------------------*/

void Isotp_Init(void)
{
    memset((void*)this,0,sizeof(Isotp_Obj_t));
    this->rx_packet.buffer = core_obj.rx_buf; // 缓冲区初始化

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
void Isotp_TimerHandler(uint8_t **ReceiveData,size_t *ReceiveDataLen,bool *ReceiveEn,bool *isPhysical)
{
    if(this->frame.en)
    {
        Isotp_ReceiveCallback();
    }

    // if(this->Uplayer.rx.en) // 从上层接收到数据
    // {
    //     Isotp_UpLayerReceive();
    // }

    *isPhysical = this->frame.isPhySical;
    *ReceiveEn = false;
    if(this->Uplayer.tx.en) // 给上层传输数据
    {
    this->Uplayer.tx.en = false;
    *ReceiveData = this->rx_packet.buffer; // 不拷贝，直接返回系统全局buffer地址
    *ReceiveDataLen = this->Uplayer.tx.len;
    *ReceiveEn = true; // 只有在此值为true才允许操作 ReceiveData
    }

    Isotp_Send_ResponseAddress(); // 发送动作不归我管，定时调用即可
}



void Isotp_FunctionAddress(uint8_t *data,size_t len)
{
    memcpy((void*)this->frame.data,data,len);
    this->frame.en = true;
    this->frame.isPhySical = false;

    // Isotp_ReceiveCallback();
}

void Isotp_PhySicalAddress(uint8_t *data,size_t len) // 物理地址
{
    memcpy((void*)this->frame.data,data,len);
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
Isotp_Status_t Isotp_Transmit(uint8_t *data,size_t len)
{
    if(data == NULL || len <= 0)
    {
        return ISOTP_ERR;
    }
    if(len <= 7)
    {
        // this->tx.data[0] =  (ISOTP_TYPE_SF << 4 | len);
        // memcpy((void*)this->tx.data,(void*)data,len);
        // this->tx_packet.Isbusy = true;
        // this->tx.en = true;
        // this->tx.len = len;

        // 单帧直接发送

        MicroUds_Trans_ResponseAddress(data,len);
        return ISOTP_OK;
    }

    if(this->tx_packet.Isbusy)
    {
        return ISOTP_BUSY;
    }
    memcpy((void*)this->tx_packet.buffer,data,len);
    this->tx_packet.total_len = len;
    this->tx_packet.index_len = 0;
    this->tx_packet.Isbusy = false;

    return ISOTP_OK;
}