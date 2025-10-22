#ifndef ISOTP_CONF_H
#define ISOTP_CONF_H

/**
 * @file Isotp_conf.h
 * @author https://github.com/xfp23/
 * @brief conf
 * @version 0.1
 * @date 2025-10-22
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#ifdef __cplusplus
extern "C"
{
#endif

// 位域低位在字节低地址，高位在字节高地址。
#define ISOTP_LITTLE_ENDIAN_BITFIELD 1

// 位域高位在字节低地址，低位在字节高地址
//#define ISOTP_BIG_ENDIAN_BITFIELD    1

#if defined(ISOTP_LITTLE_ENDIAN_BITFIELD) && ISOTP_LITTLE_ENDIAN_BITFIELD
    #define ISOTP_BITFIELD_LITTLE_ENDIAN 1
#elif defined(ISOTP_BIG_ENDIAN_BITFIELD) && ISOTP_BIG_ENDIAN_BITFIELD
    #define ISOTP_BITFIELD_BIG_ENDIAN 1
#else
    #error "Cannot determine bitfield byte order. Define ISOTP_LITTLE_ENDIAN_BITFIELD or ISOTP_BIG_ENDIAN_BITFIELD manually."
#endif



#ifdef __cplusplus
}
#endif

#endif