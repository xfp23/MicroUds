/**
 * @file MicroUds_types.h
 * @author xfp23
 * @brief 
 * @version \ref MICROUDS_VERSION
 * @date 2026-10-22
 * 
 * @copyright Copyright (c) 2026
 * 
 */
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

/**
 * @brief 服务/子功能回调的上下文
 * @note  [REVIEW] 回答原来的疑问 "上下文如何设计？应该用统一的回调函数通知？还是让用户返回NRC 0 就是正响应?":
 *        回调保持 void(ctx*) 的形式，结果通过 ctx 里的 resp_type 表达(正/负/pending)，比 "返回0=正响应" 更清晰，
 *        也能自然扩展出 PENDING。ctx 在调用前默认被设为 "负响应 + GR"，回调必须显式调用
 *        MicroUds_CtxPositive / MicroUds_CtxNegative / MicroUds_CtxPending，避免回调忘了设置而"误发正响应"。
 *        原来的 ctx 只有输出没有输入，回调根本拿不到请求数据，现在补上了输入部分。
 */
typedef struct {
    // -------- 输入(内核填写，回调只读) --------
    uint8_t sid;                  // 请求的SID
    uint8_t sub_func;             // 子功能(已去掉抑制位)，无子功能的服务为0
    MicroUds_AddrType_t addr;     // 物理/功能寻址
    const uint8_t *req;           // 请求数据，不含SID和子功能
    uint32_t req_len;             // 请求数据长度
    uint8_t session;              // 当前会话编号
    uint8_t sec_level;            // 当前已解锁的安全等级，0=未解锁
    uint32_t call_cnt;            // 第几次被调用: 0=首次，>0 表示返回过PENDING后的重入
    void *Userdata;               // 注册时传入的用户数据

    // -------- 输出(回调填写) --------
    // [REVIEW] payload 默认指向内核的响应缓冲(已跳过 RSID/子功能)，回调直接往里写即可，len=写入的字节数。
    //          原来 "用户自己提供payload指针" 的方式仍然可用: 把 payload 指向自己的静态数据，内核会拷贝(注意不要指向栈上的局部变量，
    //          但对PENDING场景，回调返回后栈已经没了，所以只能指向静态/全局数据)。
    uint8_t *payload;
    uint32_t payload_cap;         // payload 可用容量
    uint32_t len;                 // 用户传给上层的数据长度，响应发出去
    MicroUds_RespType_t resp_type;
    uint8_t nrc;
} MicroUds_ServiceCtx_t;

typedef void (*MicroUds_ServiceFunc_t)(MicroUds_ServiceCtx_t*);

typedef struct
{
    MicroUds_RespType_t type;
    uint8_t data[MICROUDS_RESPONSE_BUF_SIZE]; //  负载
    uint32_t len;
    uint8_t Inhibit;      // 抑制响应 (只对正响应有效)
    MicroUds_AddrType_t addr; // [REVIEW] 这条响应对应的请求寻址方式。原来发送时去读 pdu.addr，新请求一来就被覆盖了，所以响应自己带一份
    uint8_t apply_ssid;   // [REVIEW] 响应发出后才生效的会话切换，0=无 (0x10 的正响应必须先发出，再切会话)
    bool final;           // [REVIEW] 这是不是请求的"最终响应"。0x78 不是最终响应，它发送完成不能释放请求；最终响应发送完成(或被静默丢弃)后才释放请求
    volatile bool tx_wait;// [REVIEW] 已交给传输层，正在等 MicroUds_HwRespTxDone()
    uint32_t tx_tick;     // [REVIEW] 交给传输层的时刻，用于超时保护
    volatile bool active; // 活跃
} MicroUds_Response_t;

typedef struct
{
    MicroUds_AddrType_t addr; // 地址

    uint8_t sid; // [REVIEW] 原注释写的是"会话"，这里是服务ID
    uint8_t sub_func;  // 子功能(已去掉抑制位)
    bool suppress;     // [REVIEW] 请求抑制正响应 (suppressPosRspMsgIndicationBit)，从请求里解析，随请求保存

    uint8_t err_nrc;   // [REVIEW] RxIndication阶段就发现的错误(如请求过长)，交给服务处理流程统一回负响应

    uint8_t raw[MICROUDS_REQUEST_BUF_SIZE]; // [REVIEW] 内核拷贝的完整请求
    uint32_t raw_len;

    const uint8_t *payload; // 指向 raw 内 SID(+子功能)之后的数据
    uint32_t len;           // payload 长度
    volatile bool active;
} MicroUds_Pdu_t;

// 0. 要处理这些东西就得先得到pdu                         ✅ RxIndication 拷贝并缓存
// 1. 先实现简单的最重要的管理会话吧 Session，把切换默认会话做了，保持会话以及会话超时退出  ✅
// 2. 再去考虑如何实现服务分发 Service                    ✅
// 3. 再去考虑实现子功能查找                              ✅
// 4. 安全权限？ 发现它应该先于服务实现？也不一定吧。。。 security   ✅ 内置 0x27，服务分发时统一检查权限
// 5. 回调机制啥的就先不考虑了，先把骨架弄出来再说吧。。。   ✅ 回调上下文 + PENDING(0x78) 已有最小实现

typedef struct
{
    uint8_t ssid;       // 当前会话 [REVIEW] 约定: 这里永远存"会话编号"，不再存位掩码
    uint8_t pending_ssid; // [REVIEW] 待生效的会话(响应发出后才切)，0=无
    uint32_t mask;      // 支持会话位掩码？4个会话，掩码做宽一点，为了方便扩展,都想做成64了哈哈，没必要吧 (32位够用了)
    uint32_t hold_tick; // 保持时间 配置项目，超过多久就退出当前会话
    uint32_t last_tick; // 上一次tick值，从MicroUds的tick中获取
    volatile bool is_default; // 是否是默认会话 
} MicroUds_Session_t;

typedef enum
{
    MICROUDS_SEC_STEP_IDLE = 0,  // 没有未完成的种子
    MICROUDS_SEC_STEP_SEED_SENT, // 种子已发出，等待 sendKey
} MicroUds_SecStep_t;

typedef struct
{
    uint8_t level;                    // 当前需要的解锁等级 (最近一次请求种子的等级)
    uint8_t seed[MICROUDS_SEED_SIZE]; // 随机数种子
    uint32_t seed_len;                // 种子长度

    uint8_t key[2][MICROUDS_KEY_SIZE]; // 密钥结果 key[0]=期望密钥(由用户算法算出)，key[1]=诊断仪发来的密钥
    uint32_t key_len[2];

    MicroUds_SecStep_t step; // 状态机，用来确定流程

    uint8_t permis;       // 权限: 当前已解锁的等级，0=未解锁
    uint8_t attempts;     // [REVIEW] 连续失败次数。注意会话切换/S3超时不能清零，否则可以靠超时绕过防暴力破解
    bool delay_active;    // [REVIEW] 失败次数超限后的延时是否生效
    uint32_t delay_start; // 延时起点tick
} MicroUds_Security_t;    // 安全权限
// [REVIEW] 原来的 active 字段删除了: 27 服务现在在服务分发里同步处理，不再需要"挂起等timer处理"的状态机
//          原来的 session 字段(哪些会话能用27)删除了: 它本来就是服务的"会话权限"，放进 0x27 的服务表项里统一检查

typedef struct {
    uint8_t id;
    
    MicroUds_ServiceFunc_t callback;
    void *Userdata; 

    // [REVIEW] 子功能级权限，对应文档里的"第二层权限"。0 表示沿用服务级(会话)/不额外要求(安全)
    uint32_t session_mask; // 0=沿用服务的会话。表中存在但当前会话不允许 -> 0x7E；表中不存在 -> 0x12
    uint8_t permis;        // 额外的安全等级要求，格式同服务的 permis
} MicroUds_SubFuncTable_t;

// [REVIEW] 原来 SubFuncTable_t 和 SubFunc_t 两个结构体字段完全一样，合并为一个
typedef MicroUds_SubFuncTable_t MicroUds_SubFunc_t;

// [REVIEW] 原来 MicruUds_* 的拼写(Micru)统一改成 MicroUds_*
typedef struct {
    uint8_t sid; // 服务id

    uint8_t flags;  // MICROUDS_SERV_FLAG_xxx
    uint8_t permis; // 安全访问权限: bit7=指定位, bit6..0=等级，用 MICROUDS_PERMIS_EXACT/ATLEAST 生成
    uint32_t session_mask; // [REVIEW] 允许的会话位掩码(用 MICROUDS_SET_SESSION(x) 组合)。
                           //          原来是单个会话 MicroUds_SessionType_t，一个服务只能属于一个会话，
                           //          像 0x22 在默认+扩展会话都可用就表达不了。
    uint16_t min_len; // 完整请求长度下限(含SID)，0=不检查
    uint16_t max_len; // 完整请求长度上限(含SID)，0=不检查

    // 服务级回调: 没有子功能的服务(0x22 0x2E ...)使用；有子功能表时作为"表里没有注册回调"的缺省处理
    MicroUds_ServiceFunc_t callback;
    void *Userdata;
} MicroUds_ServiceTable_t;

typedef struct
{
    uint8_t sid;             // 服务id
    uint8_t flags;
    uint32_t session_mask;   // [REVIEW] 所属的会话(位掩码)

    uint8_t permis; // 安全访问权限
    uint16_t min_len;
    uint16_t max_len;
    MicroUds_ServiceFunc_t callback;
    void *Userdata;

    uint32_t sub_func_num; // 子功能数量

    bool is_valid; // 有效 [REVIEW] 原名 is_vaild

    MicroUds_SubFunc_t sub_func[MICROUDS_SUBFUNC_MAX_NUMBER]; // 子功能
} MicroUds_ServiceEntry_t;

typedef struct
{
    MicroUds_ServiceEntry_t serv[MICROUDS_SERVICE_MAX_NUMBER]; // 服务表，在此处注册
    uint32_t serv_num; // 服务数量

    // [REVIEW] 异步(PENDING)执行状态。active 字段删除: 一个请求是否在处理由 pdu.active 表示
    bool pending;                           // 已发出 0x78，等待服务给出最终结果
    uint8_t pending_cnt;                    // 已发 0x78 的次数
    uint32_t pending_tick;                  // 上次发 0x78 的时刻
    uint32_t call_cnt;                      // 回调已被调用的次数
    const MicroUds_ServiceEntry_t *cur;     // 正在执行的服务
    const MicroUds_SubFunc_t *cur_sub;      // 正在执行的子功能 (NULL=服务级回调)
} MicroUds_Service_t;

typedef struct
{
    MicroUds_Pdu_t pdu;
    MicroUds_Response_t resp;     // 响应
    MicroUds_Session_t session;   // 诊断会话
    MicroUds_Service_t service;   // 诊断服务 // 子功能将以链表的形式挂载在服务里？ [REVIEW] 静态数组更适合MCU，不用链表了
    MicroUds_Security_t security; // 诊断安全
    volatile uint32_t tick;       // 整个心跳滴答
    volatile bool tx_done;        // [REVIEW] ISOTP 发送完成通知(MicroUds_HwRespTxDone 只置这个标志，真正的处理放在 TimerHandler 里，避免在中断里改内核状态)
} MicroUds_Obj_t;

#ifdef __cplusplus
}
#endif

#endif