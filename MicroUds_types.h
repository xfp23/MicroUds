#ifndef MICROUDS_TYPES_H
#define MICROUDS_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "MicroUds_conf.h"
#include "MicroUds_utils.h"
#if MICROUDS_USE_MEM
#include "MicroMem.h"
#endif

#ifdef __cplusplus
extern "C"
{
#endif
// 服务标志位
// [REVIEW] HAS_SUBFUNC 的含义是 "请求的第2字节是子功能字节"，内核会据此: 解析子功能、处理抑制位、在正响应里回显子功能。
//          它和 "有没有注册子功能表(MicroUds_ServiceRegisterSubFunc)" 是两回事:
//          0x10/0x27/0x3E 有子功能字节(HAS_SUBFUNC)，但不需要子功能表，由服务级回调自己看 ctx->sub_func 处理。
#define MICROUDS_SERV_FLAG_HAS_SUBFUNC 0x01u // 请求的第2字节是子功能 (0x22 这类没有子功能的服务不要设置)
#define MICROUDS_SERV_FLAG_SUPPRESS    0x02u // 支持"抑制正响应"位(子功能bit7)，仅 HAS_SUBFUNC 的服务可设
#define MICROUDS_SERV_FLAG_FUNC_ADDR   0x04u // 允许功能寻址，不设置则功能寻址的请求被静默忽略
#define MICROUDS_SERV_FLAG_BUILTIN     0x80u // 内核内置服务(内部使用，用户不要设置)

typedef enum
{
    MICROUDS_OK,
    MICROUDS_ERR,
    MICROUDS_BUSY,
    MICROUDS_PARAM_ERR,
    MICROUDS_NOT_FOUND,
} MicroUds_Status_t;

typedef enum
{
    MICROUDS_ADDR_PHY,  // 物理地址
    MICROUDS_ADDR_FUNC, // 功能地址
} MicroUds_AddrType_t;

typedef enum
{
    MICROUDS_RESPTYPE_POSITIVE, // 正响应
    MICROUDS_RESPTYPE_NEGATIVE, // 负响应
    MICROUDS_RESPTYPE_PENDING,  // [REVIEW] 服务还没做完: 内核发 0x78，之后每个周期再次调用该回调，直到返回正/负响应
} MicroUds_RespType_t;

// typedef enum
// {
//     MICROUDS_EVENT_REQSEED, // 请求种子
// }MicroUds_Event_t; // 本来想着事件驱动的，想想可能会有不同的数据类型还是算了，下层会很复杂

typedef enum
{
    MICROUDS_SESSION_DEFAULT = 0x01,  // 默认会话 
    MICROUDS_SESSION_PROGRAM,  // 编程会话
    MICROUDS_SESSION_EXTEND,   // 扩展会话
    MICROUDS_SESSION_SECURITY, // 安全系统诊断会话
} MicroUds_SessionType_t;
// [REVIEW] 枚举值是"线上会话编号"(10 xx 里的xx)。参与位运算时必须用 MICROUDS_SET_SESSION(x) 转成位，
//          不要把"编号"和"位掩码"混存在同一个变量里 (原代码 session.ssid 有时存编号，有时存掩码，是最大的一个bug来源)。
//          OEM自定义会话(0x40~0x5F)超过了32位，目前表达不了，后续可以改成"会话表 + 下标"。

typedef enum
{
    // ---- 基础通用 NRC ----
    MICROUDS_NRC_PR = 0x00,    // 正响应 (Positive Response，通常不作为 NRC 返回)
    MICROUDS_NRC_GR = 0x10,    // GeneralReject (通用拒绝)
    MICROUDS_NRC_SNS = 0x11,   // ServiceNotSupported (服务不支持)
    MICROUDS_NRC_SFNS = 0x12,  // SubFunctionNotSupported (子功能不支持)
    MICROUDS_NRC_IMLOI = 0x13, // IncorrectMessageLengthOrInvalidFormat (报文长度错误或格式无效)
    MICROUDS_NRC_RTL = 0x14,   // ResponseTooLong (响应过长)

    // ---- 条件与状态类 NRC ----
    MICROUDS_NRC_BRR = 0x21,    // BusyRepeatRequest (系统忙，请重复请求。通常 ECU 正在忙，诊断仪可重试)
    MICROUDS_NRC_CNC = 0x22,    // ConditionsNotCorrect (条件不正确。例如未满足前置条件、未处于正确的运行状态)
    MICROUDS_NRC_RSE = 0x24,    // RequestSequenceError (请求顺序错误。例如跳过了必要的前置步骤)
    MICROUDS_NRC_ROOR = 0x31,   // RequestOutOfRange (请求超出范围。例如查询了不存在的 DID 或参数越界)
    MICROUDS_NRC_SAD = 0x33,    // SecurityAccessDenied (安全访问拒绝。未解锁安全等级或解锁失败)
    MICROUDS_NRC_IK = 0x35,     // InvalidKey (无效的密钥。在安全访问 Seed-Key 验证时密钥错误)
    MICROUDS_NRC_ENOA = 0x36,   // ExceededNumberOfAttempts (超出尝试次数。连续输错密钥被锁定)
    MICROUDS_NRC_RTDSNE = 0x37, // RequiredTimeDelayNotExpired (所需延时未到。防暴力破解的冷却时间未结束)

    // ---- 编程与刷写类 NRC (Bootloader 常用) ----
    // [REVIEW] 原来 UDT=0x7E / TDS=0x7F，与下面的 SFNSIAS/SNSIAS 数值重复了(枚举允许重复值，但语义上是错的)。
    //          标准值: UploadDownloadNotAccepted=0x70, TransferDataSuspended=0x71
    MICROUDS_NRC_UDT = 0x70,   // UploadDownloadNotAccepted (上传/下载未被接受)
    MICROUDS_NRC_TDS = 0x71,   // TransferDataSuspended (数据传输暂停)
    MICROUDS_NRC_GPF = 0x72,   // GeneralProgrammingFailure (通用编程失败，如 Flash 擦写错)
    MICROUDS_NRC_WBSC = 0x73,  // WrongBlockSequenceCounter (块序列计数器错误，常用于 34/36/37 数据传输服务) [REVIEW] 原名 WKCMP，按标准缩写改为 WBSC
    MICROUDS_NRC_RCRRP = 0x78, // RequestCorrectlyReceived-ResponsePending (请求正确接收，但响应挂起。**极为重要**：告诉诊断仪 ECU 正在忙，别超时断开，稍后会继续回复) [REVIEW] 原名 ROR，易与 ROOR 混淆，按标准缩写改为 RCRRP

    // ---- 状态切换与会话类 NRC ----
    MICROUDS_NRC_SFNSIAS = 0x7E, // SubFunctionNotSupportedInActiveSession (当前会话下子功能不支持)
    MICROUDS_NRC_SNSIAS = 0x7F,  // ServiceNotSupportedInActiveSession (当前会话下服务不支持)
} MicroUds_Nrc_t;

#ifdef __cplusplus
}
#endif

#endif
