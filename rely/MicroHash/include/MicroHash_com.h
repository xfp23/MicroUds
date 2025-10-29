/**
 * @file MicroHash_com.h
 *
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2025-10-28
 *
 * @copyright Copyright (c) 2025
 *
 */

#ifndef MICROHASH_COM_H
#define MICROHASH_COM_H

#ifdef __cplusplus
extern "C"
{
#endif

#define MICROHASH_CHECKPTR(ptr)   \
    do                            \
    {                             \
        if (ptr == NULL)          \
        {                         \
            return MICROHASH_ERR_PARAM; \
        }                         \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif