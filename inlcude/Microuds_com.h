#ifndef MICROUDS_COM_H
#define MICROUDS_COM_H

/**
 * @file MicroUds_com.h
 * @author https://github.com/xfp23
 * @brief 工具宏
 * @version 0.1
 * @date 2025-10-21
 *
 * @copyright Copyright (c) 2025
 *
 */

#ifdef __cplusplus
extern "C"
{
#endif

#define MICROUDS_CHECKPTR(ptr)         \
    do                                 \
    {                                  \
        if (ptr == NULL)               \
            return MICROUDS_ERR_PARAM; \
    } while (0)

#define MICROUDS_SAFE_CALL_TRANSMIT(buf, len)             \
    do                                                    \
    {                                                     \
        if (MicroUDS_Handle->Transmit == NULL)            \
            return MICROUDS_ERR_TRANS;                    \
        if (MicroUDS_Handle->Transmit((buf), (len)) != 0) \
            return MICROUDS_ERR_TRANS;                    \
    } while (0)

#define UDS_RESPONSE_OFFSET 0x40

#define UDS_CHECK_ECU_BUSY()                                        \
    do                                                              \
    {                                                               \
        if (MicroUDS_Handle->Ecu_sta == ECU_BUSY)                   \
        {                                                           \
            MicroUDS_NegativeResponse(UDS_NRC_BUSY_REPEAT_REQUEST); \
            return;                                                 \
        }                                                           \
    } while (0)

#define UDS_ECUSETBUSY() MicroUDS_Handle->Ecu_sta = ECU_BUSY

#define UDS_ECUCLEAR() MicroUDS_Handle->Ecu_sta = ECU_FREE

#ifdef __cplusplus
}
#endif

#endif