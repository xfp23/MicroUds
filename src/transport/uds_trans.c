/**
 * @file uds_trans_port.c
 * @author https://github.com/xfp23
 * @brief 
 * @version 0.1
 * @date 2026-04-02
 * 
 * @copyright Copyright (c) 2026
 * 
 */

 
/* Includes ------------------------------------------------------------------*/
#include "uds_trans.h"
#include "isotp.h"

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
Uds_TransPort_Obj_t Uds_TransPort = {0};
static Uds_TransPort_Obj_t const *ctx = &Uds_TransPort;
/* Private function prototypes -----------------------------------------------*/

/* Exported functions --------------------------------------------------------*/

/* Private functions ---------------------------------------------------------*/

void uds_transport_Init(void)
{
#if MICROUDS_TRANSPORT_WAY == MICROUDS_TRANSPORT_DOCAN
    Isotp_Init();
#endif
}

void uds_transport_TimerHandler(void)
{
#if MICROUDS_TRANSPORT_WAY == MICROUDS_TRANSPORT_DOCAN
    Isotp_TimerHandler(ctx->recv_data,ctx->recv_len,ctx->recv_en,ctx->IsPhy_addr);
#endif
}

void uds_transport_TickHandler(void)
{
#if MICROUDS_TRANSPORT_WAY == MICROUDS_TRANSPORT_DOCAN
    Isotp_TickHandler();
#endif
}

void uds_transport_PhySicalAddress(uint8_t *data,size_t len)
{
#if MICROUDS_TRANSPORT_WAY == MICROUDS_TRANSPORT_DOCAN
    Isotp_PhySicalAddress(data,len);
#endif
}

MicroUds_Status_t uds_transport_Transmit(uint8_t *data,size_t len)
{
#if MICROUDS_TRANSPORT_WAY == MICROUDS_TRANSPORT_DOCAN
    Isotp_Status_t ret = Isotp_Transmit(data,len);

    if(ret == ISOTP_ERR)
    {
        return MICROUDS_ERR;
    } else if(ret == ISOTP_BUSY)
    {
        return MICROUDS_BUSY;
    }

    return MICROUDS_OK;
#endif
}

void uds_transport_FunctionAddress(uint8_t *data,size_t len)
{
#if MICROUDS_TRANSPORT_WAY == MICROUDS_TRANSPORT_DOCAN
    Isotp_FunctionAddress(data,len);
#endif
}

void uds_transport_TransmitCallback(void)
{
#if MICROUDS_TRANSPORT_WAY == MICROUDS_TRANSPORT_DOCAN
    Isotp_SendSuccess();
#endif
}

void uds_transport_ManageFcWait(bool iswait)
{
#if MICROUDS_TRANSPORT_WAY == MICROUDS_TRANSPORT_DOCAN
    Isotp_ReceiveFcWait(iswait);
#endif
}

