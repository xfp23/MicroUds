#ifndef ISOTP_COM_H
#define ISOTP_COM_H

#ifdef __cplusplus
extern "C"
{
#endif


#define ISOTP_PACKSF(data, size) do { \
    Isotp_Handle->SF.byte.PCIType = FRAME_SINGLE; \
    Isotp_Handle->SF.byte.PCI_DL  = (uint8_t)(size & 0x0F); \
    memcpy(Isotp_Handle->SF.byte.Payload, data, size); \
} while(0)

#define ISOTP_CLEARSF() memset(Isotp_Handle->SF.data, 0, sizeof(Isotp_Handle->SF.data))

#ifdef __cplusplus
}
#endif

#endif