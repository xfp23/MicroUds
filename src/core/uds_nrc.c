/**
 * @file    uds_nrc.c
 * @author  https://github.com/xfp23
 * @version 0.1
 * @date    2026/04/02
 */

/* Includes ------------------------------------------------------------------*/
#include "uds_nrc.h"
#include "uds_trans.h"

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

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

MicroUds_Status_t Uds_Nrc_PositiveResponse(uint8_t sid,uint8_t *data,size_t len)
{
    // if(len >= MICROUDS_TX_BUFFER_MEMORY_SIZE)
    // {
    //     return MICROUDS_BUFF_OVERFLOW;
    // }

    // if()
    // uds_transport_Transmit();
}

