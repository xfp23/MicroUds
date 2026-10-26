/**
 * @file MicroUdsServer_types.h
 * @author xfp23
 * @brief
 * @version \ref MICROUDS_VERSION
 * @date 2026-10-22
 *
 * @copyright Copyright (c) 2026
 *
 */
#ifndef MICROUDSSERVER_TYPES_H
#define MICROUDSSERVER_TYPES_H

#include "MicroUds_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum {
    MICROUDS_SERVER_REQ_HANDLE, // 处理一个请求前
    MICROUDS_SERVER_RESP_HANDLE, // 发送一个响应后
} MicroUdsServer_Event_t;

/**
 * @brief 服务/子功能回调的上下文
 * @note  [REVIEW] 回答原来的疑问 "上下文如何设计？应该用统一的回调函数通知？还是让用户返回NRC 0 就是正响应?":
 *        回调保持 void(ctx*) 的形式，结果通过 ctx 里的 resp_type 表达(正/负/pending)，比 "返回0=正响应" 更清晰，
 *        也能自然扩展出 PENDING。ctx 在调用前默认被设为 "负响应 + GR"，回调必须显式调用
 *        MicroUdsServer_CtxPositive / MicroUdsServer_CtxNegative / MicroUdsServer_CtxPending，避免回调忘了设置而"误发正响应"。
 *        原来的 ctx 只有输出没有输入，回调根本拿不到请求数据，现在补上了输入部分。
 */
typedef struct
{
    // -------- 输入(内核填写，回调只读) --------
    uint8_t sid;              // 请求的SID
    uint8_t sub_func;         // 子功能(已去掉抑制位)，无子功能的服务为0
    MicroUds_AddrType_t addr; // 物理/功能寻址
    // MicroUdsServer_Event_t event; //
    const uint8_t *req;       // 请求数据，不含SID和子功能
    uint32_t req_len;         // 请求数据长度
    uint8_t session;          // 当前会话编号
    uint8_t sec_level;        // 当前已解锁的安全等级，0=未解锁
    uint32_t call_cnt;        // 第几次被调用: 0=首次，>0 表示返回过PENDING后的重入
    void *Userdata;           // 注册时传入的用户数据

    // -------- 输出(回调填写) --------
    // [REVIEW] payload 默认指向内核的响应缓冲(已跳过 RSID/子功能)，回调直接往里写即可，len=写入的字节数。
    //          原来 "用户自己提供payload指针" 的方式仍然可用: 把 payload 指向自己的静态数据，内核会拷贝(注意不要指向栈上的局部变量，
    //          但对PENDING场景，回调返回后栈已经没了，所以只能指向静态/全局数据)。
    uint8_t *payload;
    uint32_t payload_cap; // payload 可用容量
    uint32_t len;         // 用户传给上层的数据长度，响应发出去
    MicroUds_RespType_t resp_type;
    uint8_t nrc;
} MicroUdsServer_ServiceCtx_t;

typedef void (*MicroUdsServer_ServiceFunc_t)(MicroUdsServer_ServiceCtx_t *);

typedef struct
{
    MicroUds_RespType_t type;
    uint8_t data[MICROUDS_RESPONSE_BUF_SIZE]; //  负载
    uint32_t len;
    uint8_t Inhibit;          // 抑制响应 (只对正响应有效)
    MicroUds_AddrType_t addr; // [REVIEW] 这条响应对应的请求寻址方式。原来发送时去读 pdu.addr，新请求一来就被覆盖了，所以响应自己带一份
    uint8_t apply_ssid;       // [REVIEW] 响应发出后才生效的会话切换，0=无 (0x10 的正响应必须先发出，再切会话)
    bool final;               // [REVIEW] 这是不是请求的"最终响应"。0x78 不是最终响应，它发送完成不能释放请求；最终响应发送完成(或被静默丢弃)后才释放请求
    volatile bool tx_wait;    // [REVIEW] 已交给传输层，正在等 MicroUdsServer_HwRespTxDone()
    uint32_t tx_tick;         // [REVIEW] 交给传输层的时刻，用于超时保护
    volatile bool active;     // 活跃
} MicroUdsServer_Response_t;

typedef struct
{
    MicroUds_AddrType_t addr; // 地址

    uint8_t sid;      // [REVIEW] 原注释写的是"会话"，这里是服务ID
    uint8_t sub_func; // 子功能(已去掉抑制位)
    bool suppress;    // [REVIEW] 请求抑制正响应 (suppressPosRspMsgIndicationBit)，从请求里解析，随请求保存

    uint8_t err_nrc; // [REVIEW] RxIndication阶段就发现的错误(如请求过长)，交给服务处理流程统一回负响应

    uint8_t raw[MICROUDS_REQUEST_BUF_SIZE]; // [REVIEW] 内核拷贝的完整请求
    uint32_t raw_len;

    const uint8_t *payload; // 指向 raw 内 SID(+子功能)之后的数据
    uint32_t len;           // payload 长度
    volatile bool active;
} MicroUdsServer_Pdu_t;

// 0. 要处理这些东西就得先得到pdu                         RxIndication 拷贝并缓存
// 1. 先实现简单的最重要的管理会话吧 Session，把切换默认会话做了，保持会话以及会话超时退出
// 2. 再去考虑如何实现服务分发 Service
// 3. 再去考虑实现子功能查找
// 4. 安全权限？ 发现它应该先于服务实现？也不一定吧。。。 security   内置 0x27，服务分发时统一检查权限
// 5. 回调机制啥的就先不考虑了，先把骨架弄出来再说吧。。。   回调上下文 + PENDING(0x78) 已有最小实现

typedef struct
{
    uint8_t ssid;             // 当前会话 [REVIEW] 约定: 这里永远存"会话编号"，不再存位掩码
    uint8_t pending_ssid;     // [REVIEW] 待生效的会话(响应发出后才切)，0=无
    uint32_t mask;            // 支持会话位掩码？4个会话，掩码做宽一点，为了方便扩展,都想做成64了哈哈，没必要吧 (32位够用了)
    uint32_t hold_tick;       // 保持时间 配置项目，超过多久就退出当前会话
    uint32_t last_tick;       // 上一次tick值，从MicroUdsServer的tick中获取
    volatile bool is_default; // 是否是默认会话
} MicroUdsServer_Session_t;

typedef enum
{
    MICROUDS_SEC_STEP_IDLE = 0,  // 没有未完成的种子
    MICROUDS_SEC_STEP_SEED_SENT, // 种子已发出，等待 sendKey
} MicroUdsServer_SecStep_t;

typedef struct
{
    uint8_t level;                    // 当前需要的解锁等级 (最近一次请求种子的等级)
    uint8_t seed[MICROUDS_SEED_SIZE]; // 随机数种子
    uint32_t seed_len;                // 种子长度

    uint8_t key[2][MICROUDS_KEY_SIZE]; // 密钥结果 key[0]=期望密钥(由用户算法算出)，key[1]=诊断仪发来的密钥
    uint32_t key_len[2];

    MicroUdsServer_SecStep_t step; // 状态机，用来确定流程

    uint8_t permis;          // 权限: 当前已解锁的等级，0=未解锁
    uint8_t attempts;        // [REVIEW] 连续失败次数。注意会话切换/S3超时不能清零，否则可以靠超时绕过防暴力破解
    bool delay_active;       // [REVIEW] 失败次数超限后的延时是否生效
    uint32_t delay_start;    // 延时起点tick
} MicroUdsServer_Security_t; // 安全权限
// [REVIEW] 原来的 active 字段删除了: 27 服务现在在服务分发里同步处理，不再需要"挂起等timer处理"的状态机
//          原来的 session 字段(哪些会话能用27)删除了: 它本来就是服务的"会话权限"，放进 0x27 的服务表项里统一检查

typedef struct
{
    uint8_t id;

    MicroUdsServer_ServiceFunc_t callback;
    void *Userdata;

    // [REVIEW] 子功能级权限，对应文档里的"第二层权限"。0 表示沿用服务级(会话)/不额外要求(安全)
    uint32_t session_mask; // 0=沿用服务的会话。表中存在但当前会话不允许 -> 0x7E；表中不存在 -> 0x12
    uint8_t permis;        // 额外的安全等级要求，格式同服务的 permis
} MicroUdsServer_SubFuncTable_t;

// [REVIEW] 原来 SubFuncTable_t 和 SubFunc_t 两个结构体字段完全一样，合并为一个
typedef MicroUdsServer_SubFuncTable_t MicroUdsServer_SubFunc_t;

// [REVIEW] 原来 MicruUds_* 的拼写(Micru)统一改成 MicroUdsServer_*
typedef struct
{
    uint8_t sid; // 服务id

    uint8_t flags;         // MICROUDS_SERV_FLAG_xxx
    uint8_t permis;        // 安全访问权限: bit7=指定位, bit6..0=等级，用 MICROUDS_PERMIS_EXACT/ATLEAST 生成
    uint32_t session_mask; // [REVIEW] 允许的会话位掩码(用 MICROUDS_SET_SESSION(x) 组合)。
                            //          原来是单个会话 MicroUdsServer_SessionType_t，一个服务只能属于一个会话，
                            //          像 0x22 在默认+扩展会话都可用就表达不了。
    uint16_t min_len;      // 完整请求长度下限(含SID)，0=不检查
    uint16_t max_len;      // 完整请求长度上限(含SID)，0=不检查

    // 服务级回调: 没有子功能的服务(0x22 0x2E ...)使用；有子功能表时作为"表里没有注册回调"的缺省处理
    MicroUdsServer_ServiceFunc_t callback;
    void *Userdata;
} MicroUdsServer_ServiceTable_t;

typedef struct
{
    uint8_t sid; // 服务id
    uint8_t flags;
    uint32_t session_mask; // [REVIEW] 所属的会话(位掩码)

    uint8_t permis; // 安全访问权限
    uint16_t min_len;
    uint16_t max_len;
    MicroUdsServer_ServiceFunc_t callback;
    void *Userdata;

    uint32_t sub_func_num; // 子功能数量

    bool is_valid; // 有效 [REVIEW] 原名 is_vaild

    MicroUdsServer_SubFunc_t sub_func[MICROUDS_SUBFUNC_MAX_NUMBER]; // 子功能
} MicroUdsServer_ServiceEntry_t;

typedef struct
{
    MicroUdsServer_ServiceEntry_t serv[MICROUDS_SERVICE_MAX_NUMBER]; // 服务表，在此处注册
    uint32_t serv_num;                                               // 服务数量

    // [REVIEW] 异步(PENDING)执行状态。active 字段删除: 一个请求是否在处理由 pdu.active 表示
    bool pending;                             // 已发出 0x78，等待服务给出最终结果
    uint8_t pending_cnt;                      // 已发 0x78 的次数
    uint32_t pending_tick;                    // 上次发 0x78 的时刻
    uint32_t call_cnt;                        // 回调已被调用的次数
    const MicroUdsServer_ServiceEntry_t *cur; // 正在执行的服务
    const MicroUdsServer_SubFunc_t *cur_sub;  // 正在执行的子功能 (NULL=服务级回调)
} MicroUdsServer_Service_t;

typedef struct
{
    MicroUdsServer_Pdu_t pdu;
    MicroUdsServer_Response_t resp;     // 响应
    MicroUdsServer_Session_t session;   // 诊断会话
    MicroUdsServer_Service_t service;   // 诊断服务 // 子功能将以链表的形式挂载在服务里？ [REVIEW] 静态数组更适合MCU，不用链表了
    MicroUdsServer_Security_t security; // 诊断安全
    volatile uint32_t tick;             // 整个心跳滴答
    volatile bool tx_done;              // [REVIEW] ISOTP 发送完成通知(MicroUdsServer_HwRespTxDone 只置这个标志，真正的处理放在 TimerHandler 里，避免在中断里改内核状态)
} MicroUdsServer_Obj_t;

#ifdef __cplusplus
}
#endif

#endif