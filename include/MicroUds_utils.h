#ifndef MICROUDS_UTILS_H
#define MICROUDS_UTILS_H

#ifdef __cplusplus
extern "C"
{
#endif

#define MICROUDS_GET_RESPINHIBIT(x) (x & 0x80)

#define MICROUDS_SET_SESSION(x) ((uint32_t)((1U << (x)) - 1U))

#ifdef __cplusplus
}
#endif

#endif
