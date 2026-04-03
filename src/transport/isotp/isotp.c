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

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
static Isotp_Obj_t isotp_obj = {0};
static Isotp_Obj_t *const this = &isotp_obj;
/* Private function prototypes -----------------------------------------------*/

/* Exported functions --------------------------------------------------------*/

/* Private functions ---------------------------------------------------------*/

void Isotp_Init(void)
{
    memset((void*)this,0,sizeof(Isotp_Obj_t));
    this->buffer = core_obj.buffer; // 缓冲区初始化
}

void Isotp_Tick(void)
{
    this->tick++;
}

void Isotp_TimerHandler(uint8_t *data,size_t len,bool *isPhysical)
{

}

void Isotp_Send_ResponseAddress(uint8_t *data,size_t len)
{

}

void Isotp_FunctionAddress(uint8_t *data,size_t len)
{

}