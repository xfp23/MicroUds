/**
 * @file uds_trans_port.h
 * @author https://github.com/xfp23
 * @brief 
 * @version 0.1
 * @date 2026-04-02
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef UDS_TRANS_H
#define UDS_TRANS_H

#include "MicroUds_conf.h"
#include "MicroUds_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct 
{
    uint8_t *recv_data;
    size_t recv_len;

    bool recv_en;
    bool IsPhy_addr;

}Uds_TransPort_Obj_t;

extern Uds_TransPort_Obj_t Uds_TransPort;

/**
 * @brief Init Microuds transport
 * 
 */
extern void uds_transport_Init(void);

extern void uds_transport_TimerHandler(void);

extern void uds_transport_TickHandler(void);

extern void uds_transport_ManageFcWait(bool iswait);
 
extern void uds_transport_PhySicalAddress(uint8_t *data,size_t len);

extern void uds_transport_Transmit(uint8_t *data,size_t len);

extern void uds_transport_FunctionAddress(uint8_t *data,size_t len);

extern void uds_transport_TransmitCallback(void);

#ifdef __cplusplus
}
#endif

#endif /* UDS_TRANS_H */