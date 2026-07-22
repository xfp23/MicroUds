/**
 * @file    uds_dispatch.c
 * @author  https://xfp23.github.io
 * @version 0.1
 * @date    2026/04/02
 */

/* Includes ------------------------------------------------------------------*/
#include "uds_dispatch.h"
// #include "uds_context.h"
#include "uds_core.h"
#include "uds_trans.h"
#include "uds_nrc.h"

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
Uds_Dispatch_Obj_t uds_dispatch_obj = {0};
static Uds_Dispatch_Obj_t *const ctx = &uds_dispatch_obj;
/* Private function prototypes -----------------------------------------------*/

/* Exported functions --------------------------------------------------------*/

/* Private functions ---------------------------------------------------------*/

void uds_dispatch_timerhandler()
{
    if (Uds_TransPort.recv_en == false)
    {
        return;
    }

    if (Uds_TransPort.recv_len == 0)
    {
        return;
    }

    ctx->sid = Uds_TransPort.recv_data[0];
    if (Uds_Context_FindService(ctx->sid, &ctx->service) == MICROUDS_OK)
    {
        if (Uds_TransPort.IsPhy_addr == false)
        { // 功能地址
            if (ctx->service.IsAckFuncAddress == true)
            {
                ctx->service.func(&Uds_TransPort.recv_data[1], Uds_TransPort.recv_len);
            } // 此处就静默，当作什么都没发生
        }
        else
        { // 物理地址
            ctx->service.func(&Uds_TransPort.recv_data[1], Uds_TransPort.recv_len);
        }
    }
    else
    {
        // 返回一个负响应
        Uds_Nrc_NegativeResponse(ctx->sid, UDS_NRC_SERVICE_NOT_SUPPORTED);
    }
}
