#ifndef MICROUDS_TYPES_H
#define MICROUDS_TYPES_H

#include "stdbool.h"
#include "string.h"
#include "stdlib.h"
#include "MicroUds_conf.h"
#include "MicroUds_utils.h"
#include "stdint.h"
#if MICROUDS_USE_MEM
#include "MicroMem.h"
#endif

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum
{
    MICROUDS_OK,
    MICROUDS_ERR,
    MICROUDS_BUSY,
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
} MicroUds_Session_t;

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

    // ---- 状态切换与会话类 NRC ----
    MICROUDS_NRC_SFNSIAS = 0x7E, // SubFunctionNotSupportedInActiveSession (当前会话下子功能不支持)
    MICROUDS_NRC_SNSIAS = 0x7F,  // ServiceNotSupportedInActiveSession (当前会话下服务不支持)

    // ---- 编程与刷写类 NRC (Bootloader 常用) ----
    MICROUDS_NRC_GPF = 0x72,   // GeneralProgrammingFailure (通用编程失败，如 Flash 擦写错)
    MICROUDS_NRC_WKCMP = 0x73, // WrongBlockSequenceCounter (块序列计数器错误，常用于 34/36/37 数据传输服务)
    MICROUDS_NRC_ROR = 0x78,   // RequestCorrectlyReceived-ResponsePending (请求正确接收，但响应挂起。**极为重要**：告诉诊断仪 ECU 正在忙，别超时断开，稍后会继续回复)
    MICROUDS_NRC_UDT = 0x7E,   // UploadDownloadNotAccepted (上传/下载未被接受)
    MICROUDS_NRC_TDS = 0x7F    // TransferDataSuspended (数据传输暂停)
} Uds_Nrc_t;

typedef struct
{
    MicroUds_RespType_t type;
    uint8_t data[MICROUDS_RESPONSE_BUF_SIZE]; //  负载
    uint32_t len;
    uint8_t Inhibit;      // 抑制响应
    volatile bool active; // 活跃
} MicroUds_Response_t;

typedef struct
{
    MicroUds_AddrType_t addr; // 地址

    uint8_t ssid; // 会话
    uint8_t sid;  // 服务

    uint8_t *payload;
    uint32_t len;
    volatile bool active;
} MicroUds_Pdu_t;

// 0. 要处理这些东西就得先得到pdu
// 1. 先实现简单的最重要的管理会话吧 Session，把切换默认会话做了，保持会话以及会话超时退出
// 2. 再去考虑如何实现服务分发 Service
// 3. 再去考虑实现子功能查找
// 4. 安全权限？ 发现它应该先于服务实现？也不一定吧。。。 security
// 5. 回调机制啥的就先不考虑了，先把骨架弄出来再说吧。。。

typedef struct
{
    uint8_t ssid;       // 当前会话
    uint32_t mask;      // 支持会话位掩码？4个会话，掩码做宽一点，为了方便扩展,都想做成64了哈哈，没必要吧
    uint32_t hold_tick; // 保持时间 配置项目，超过多久就退出当前会话
    uint32_t last_tick; // 上一次tick值，从MicroUds的tick中获取
} MicroUds_Session_t;

typedef struct
{
    uint8_t sid;             // 服务id
    MicroUds_Session_t ssid; // 所属的会话

    uint32_t permis; // 访问权限
} MicruUds_ServiceSub_t;

typedef struct
{
    uint8_t level;                    // 当前需要的解锁等级
    uint8_t seed[MICROUDS_SEED_SIZE]; // 随机数种子
    uint32_t seed_len; // 种子长度
    uint32_t session;                 // 会话访问

    uint8_t key[2][MICROUDS_KEY_SIZE]; // 密钥结果
    uint32_t key_len[2]; 

    uint8_t step; // 状态机，用来确定流程

    uint32_t permis;      // 权限
    volatile bool active; // 活跃
} MicroUds_Security_t;    // 安全权限

typedef struct
{
    uint8_t req_sid; // 当前请求的sid

} MicruUds_Service_t;

typedef struct
{
    MicroUds_Pdu_t pdu;
    MicroUds_Response_t resp;     // 响应
    MicroUds_Session_t session;   // 诊断会话
    MicruUds_Service_t service;   // 诊断服务 // 子功能将以链表的形式挂载在服务里？
    MicroUds_Security_t security; // 诊断安全
    volatile uint32_t tick;       // 整个心跳滴答
} MicroUds_Obj_t;

#ifdef __cplusplus
}
#endif

#endif
