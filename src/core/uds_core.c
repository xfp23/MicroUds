/**
 * @file    uds_core.c
 * @author  https://github.com/xfp23
 * @version 0.1
 * @date    2026/04/02
 */

/* Includes ------------------------------------------------------------------*/
#include "uds_core.h"
#include "uds_trans.h"

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
Uds_Core_t core_obj = {0};
/* Private function prototypes -----------------------------------------------*/

/* Exported functions --------------------------------------------------------*/

/* Private functions ---------------------------------------------------------*/

void MicroUds_Init(void)
{
    uds_transport_Init(); // 传输层初始化
}


void MicroUds_TickHandler(void)
{
    uds_transport_TickHandler(); // 传输层滴答回调

}

void MicroUds_TimerHandler(void)
{
    uds_transport_TimerHandler(); // 传输层回调

}

