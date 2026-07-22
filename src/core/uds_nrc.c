/**
 * @file    uds_nrc.c
 * @author  https://xfp23.github.io
 * @version 0.1
 * @date    2026/04/02
 */

/* Includes ------------------------------------------------------------------*/
#include "uds_nrc.h"
#include "uds_trans.h"
#include "string.h"

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
static Uds_NrcObj_t uds_nrc_obj = {0};
static Uds_NrcObj_t *const ctx = &uds_nrc_obj;
/* Private function prototypes -----------------------------------------------*/

/* Exported functions --------------------------------------------------------*/

/* Private functions ---------------------------------------------------------*/
MicroUds_Status_t Uds_Nrc_NegativeResponse(uint8_t sid,Uds_NrcCode_t code)
{
    uint8_t data[3] = {0};
    data[0] = 0x7F;
    data[1] = sid;
    data[2] = code;
    uds_transport_Transmit(data,3);

    return MICROUDS_OK;
}

MicroUds_Status_t Uds_Nrc_PositiveResponse(uint8_t sid, uint16_t ssid, uint8_t *data, size_t len)
{
    if (len >= MICROUDS_TX_BUFFER_MEMORY_SIZE)
    {
        return MICROUDS_BUFF_OVERFLOW;
    }

    memset((void *)ctx->PostRes_buf, 0, MICROUDS_NRC_BUFFER_SIZE);
    size_t total_len = 0;
    ctx->PostRes_buf[0] = sid + 0x40;
    if (ssid > 0xFF)
    {
        ctx->PostRes_buf[1] = (uint8_t)((ssid & 0xF0) >> 8);
        ctx->PostRes_buf[2] = (uint8_t)((ssid & 0x0F));
        total_len = 3;

        if (data != NULL && len != 0)
        {
            memcpy((void *)&ctx->PostRes_buf[3], data, len);
            total_len += len;
        }
    }
    else
    {
        ctx->PostRes_buf[1] = ssid;
        total_len = 2;
        if (data != NULL && len != 0)
        {
            memcpy((void *)&ctx->PostRes_buf[2], data, len);
            total_len += len;
        }
    }

    return uds_transport_Transmit(ctx->PostRes_buf, total_len);
}
