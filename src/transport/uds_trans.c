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

/* Private function prototypes -----------------------------------------------*/

/* Exported functions --------------------------------------------------------*/

/* Private functions ---------------------------------------------------------*/

void uds_Trans_Init(void)
{
    Isotp_Init();
}

void uds_Trans_TimerHandler()
{

}