/**
 * @file MicroHash_conf.h
 * @author 
 * @brief hash表的配置
 * @version 0.1
 * @date 2025-10-28
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef MICROHASH_CONF_H
#define MICROHASH_CONF_H

#ifdef __cplusplus
extern  "C"
{
#endif

#define MICROHASH_VERSION_MACR "0.0.1"

#define MICROHASH_IDBITS 8

#define MICROHASH_BUCKETS_SIZE(x) (1u << x)


#ifdef __cplusplus
}
#endif

#endif //MICROHASH_CONF_H