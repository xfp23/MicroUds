/**
 * @file MicroUds_utils.h
 * @author xfp23
 * @brief 
 * @version 0.2
 * @date 2026-10-22
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef MICROUDS_UTILS_H
#define MICROUDS_UTILS_H

#ifdef __cplusplus
extern "C"
{
#endif

// [REVIEW] 参数加了括号；返回布尔。注意：只有声明了 MICROUDS_SERV_FLAG_SUPPRESS 的服务才能按这个位去解释子功能
#define MICROUDS_GET_RESPINHIBIT(x) (((x) & 0x80U) != 0U)

// [REVIEW] 原来是 ((1U << x) - 1U)，那是"低于x的所有位都置1"，用来表示单个会话是错的:
//   ssid=3 时得到 0b111，而不是 0b1000。 会话位掩码应该是 "第x位"，x 取值 0..31
#define MICROUDS_SET_SESSION(x) ((uint32_t)1u << (x))

// [REVIEW] 生成 "支持会话 1..n" 的掩码 (bit1..bitn)，会话编号从1开始，所以不含bit0。n=4 -> 0x1E
#define MICROUDS_SESSION_MASK_UPTO(n) (((uint32_t)1u << ((n) + 1u)) - 2u)

// 服务权限字节: bit7 = 指定位(1: 必须"恰好等于"该等级; 0: 已解锁等级 >= 该等级即可)，bit6..0 = 等级，0 表示无需解锁
#define MICROUDS_GET_ASSIG(x) (((x) & 0x80U) != 0U)
#define MICROUDS_PERMIS_LEVEL(x) ((uint8_t)((x) & 0x7FU))
#define MICROUDS_PERMIS_EXACT(level) ((uint8_t)((level) | 0x80U)) // 配置服务时的辅助宏: 仅等级level可访问
#define MICROUDS_PERMIS_ATLEAST(level) ((uint8_t)((level) & 0x7FU)) // 配置服务时的辅助宏: 等级>=level可访问

// [REVIEW] 回绕安全的超时判断: now - start 用无符号减法，tick溢出也正确
#define MICROUDS_TICK_ELAPSED(now, start, dur) (((uint32_t)((now) - (start))) >= (uint32_t)(dur))

#define MICROUDS_SIZEOF(x) ((uint32_t)(sizeof(x) / sizeof(x[0])))

#ifdef __cplusplus
}
#endif

#endif