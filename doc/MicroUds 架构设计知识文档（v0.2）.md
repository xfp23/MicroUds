# MicroUds 架构设计知识文档（v0.2）

基于 v0.1 讨论稿的审阅、遗漏补充与数据结构整理 · 2026-10-20

## 0. 摘要

MicroUds 是一个独立的 UDS 协议内核：只处理完整 PDU，不关心 CAN 分段与流控（由 MicroIostp 负责）。服务以插件形式注册到一张扁平注册表；会话、安全解锁状态、响应规则、定时器、请求执行状态由内核集中维护；服务只声明访问策略并实现业务回调。

v0.1 的主干方向正确：内核与服务分离、扁平注册表、策略与运行时状态分离、服务/子功能/数据对象三层权限、P2/P2\*/S3/业务期限四类定时、MainFunction 非阻塞驱动。本文补充 v0.1 中尚未落地的部分，重点是数据结构。

> 说明：NRC 优先级、功能寻址静默规则、P2/P2\* 取值等细节，请以 ISO 14229-1 及所在 OEM 规范为准；本文给出的是常见实践与设计建议。

## 1. 遗漏与补充清单

| # | 遗漏项 | 为什么重要 | 建议 |
| --- | --- | --- | --- |
| 1 | 发送确认（TxConfirm） | 0x10 的会话切换、0x11 的复位必须在正响应真正发出之后生效；S3 也宜从响应发送完成起计时 | 传输适配层提供发送完成回调，内核增加 Tx 状态 |
| 2 | 缓冲区所有权 | 请求指针在 RxPdu 返回后可能失效，而 0x78 之后的异步服务仍要读请求；响应缓冲区谁提供、多大未定 | 内核持有 rx/tx 缓冲区，请求入口即拷贝；响应带容量，超出返回 0x14 |
| 3 | 校验顺序与 NRC 优先级 | v0.1 的流程把长度校验放在会话之前，而常见做法是先判服务/会话/子功能，再判完整长度 | 校验做成固定阶段的流水线（见第 4 节），并写入测试用例 |
| 4 | 功能寻址静默规则未落地 | 功能寻址下，常见的 0x11、0x12、0x31、0x7E、0x7F 负响应需要静默 | 在统一的负响应出口集中处理，业务服务不感知 |
| 5 | 忙状态下收到新请求 | 0x78 之后或执行期间来了新请求，v0.1 只说不能直接覆盖，没有给策略 | TesterPresent 可处理（刷新 S3）；其他请求按配置返回 0x21 或丢弃 |
| 6 | 多 tester / 多通道 | 全局变量会让多连接场景无法扩展 | 所有状态放进 MicroUds\_Ctx 实例，一个诊断连接一个实例 |
| 7 | 会话硬编码 4 位 | OEM 自定义会话（如 0x40-0x5F）无法表达 | 会话表驱动：掩码位 = 会话表下标，线上值通过表映射 |
| 8 | 会话切换副作用清单 | 切会话不只是改变量 | 统一流程：中止运行中的服务、锁安全、清 seed 状态、onExit/onEnter、重置 S3 |
| 9 | 安全访问细节 | 种子/密钥流程有大量状态 | 每个安全等级一份运行时状态（见 3.6） |
| 10 | 0x22 多 DID 与响应长度 | 多个 DID 合计可能超出缓冲区；部分可读部分不可读时不能返回半截 | 先校验全部 DID 与长度，再统一读取；超长返回 0x14 |
| 11 | 0x78 策略参数 | 无限发 0x78 等于无期限占用 | 配置最大 Pending 次数与发送提前量，超限由内核终止服务并返回最终负响应 |
| 12 | 回调约束 | 回调阻塞或重入会破坏状态机 | 规定：回调非阻塞、不得在回调内调用 RxPdu、明确调用上下文（任务/中断） |
| 13 | 时基与溢出 | 32 位毫秒计数器约 49 天回绕 | 用差值比较，不直接比较绝对值（见 3.8） |
| 14 | 条件钩子 | ConditionsNotCorrect（0x22）常依赖车速、电压等项目状态 | 服务与数据对象各提供可选的 preCondition 回调 |
| 15 | 配置静态校验 | 手写表容易重复或越界 | 生成器检查：SID/DID/RID 重复、表排序、掩码引用合法、缓冲区容量足够 |
| 16 | 可观测性 | 现场问题难复现 | 可选 trace 钩子：收到请求、选定服务、最终 NRC、状态迁移 |
| 17 | 扩展点 | 后续会接 DTC（0x19/0x14）、0x28、0x85、0x34-0x37 | 预留 onSessionChange 广播与通用服务回调，内核不为这些服务写特例 |
| 18 | 资源裁剪 | MCU 资源有限 | 编译期宏裁剪子功能表、DID 表、trace、多实例数量 |

## 2. 分层与模块

```
应用服务模块：0x22 / 0x2E / 0x31 / 0x34-0x37 / DTC 等（仅声明策略 + 业务回调）
        |  注册（静态表）
MicroUds Core
  - 请求入口与缓冲  - 校验流水线  - 服务/子功能查找
  - 会话管理       - 安全访问状态  - 响应与 NRC 出口
  - 定时器 P2/P2*/S3/服务期限  - 请求执行状态机  - Tx 状态
  - 内置服务：0x10 / 0x27 / 0x3E（0x11 视项目内置）
        |  传输适配接口（回调注入）
MicroIostp（分段、重组、流控）
```

## 3. 核心数据结构

### 3.1 基础类型与返回值

```c
typedef uint32_t MicroUds_SessionMask;  /* 位 = 会话表下标 */
typedef uint32_t MicroUds_SecMask;      /* 位 = 安全等级表下标 */
typedef uint8_t  MicroUds_Nrc;

typedef enum { MICRO_UDS_ADDR_PHYSICAL = 0, MICRO_UDS_ADDR_FUNCTIONAL } MicroUds_AddrType;

typedef enum {
    MICRO_UDS_RET_DONE = 0,     /* 已完成，内核发送最终正响应 */
    MICRO_UDS_RET_PENDING,      /* 未完成，内核按需发 0x78，之后继续调用 process */
    MICRO_UDS_RET_NRC,          /* 失败，NRC 写入 resp->nrc */
    MICRO_UDS_RET_NO_RESPONSE   /* 明确不响应（例如服务自行判定的静默场景） */
} MicroUds_Ret;

typedef enum {
    MICRO_UDS_ABORT_SESSION_CHANGE,
    MICRO_UDS_ABORT_S3_TIMEOUT,
    MICRO_UDS_ABORT_SERVICE_DEADLINE,
    MICRO_UDS_ABORT_PENDING_LIMIT,
    MICRO_UDS_ABORT_RESET
} MicroUds_AbortReason;
```

### 3.2 统一访问策略（服务、子功能、DID、RID 共用）

```c
typedef struct {
    MicroUds_SessionMask sessionMask;  /* 0 = 任何会话都不可用 */
    MicroUds_SecMask     secMask;      /* 0 = 不要求解锁；非 0 = 命中任一已解锁等级即可 */
    uint8_t              flags;        /* ACCESS_REQUIRE_ALL_SEC：要求 secMask 内全部解锁 */
    bool (*extraCheck)(const struct MicroUds_Ctx *ctx);  /* 可选，复杂组合策略 */
} MicroUds_AccessPolicy;
```

一个结构复用四处，生成器与检查逻辑只写一份。

### 3.3 请求、响应

```c
typedef struct {
    const uint8_t    *data;      /* 完整 PDU，含 SID */
    uint16_t          length;
    MicroUds_AddrType addrType;
    uint8_t           channelId; /* 多通道时使用 */
} MicroUds_RxPdu;               /* 仅在 RxPdu 调用期间有效，内核立即拷贝 */

typedef struct {
    uint8_t           sid;
    bool              hasSubFunction;
    uint8_t           subFunction;       /* 已去掉抑制位 */
    bool              suppressPosRsp;    /* 仅对声明支持的服务有效 */
    const uint8_t    *payload;           /* SID（及子功能）之后的数据，指向内核 rx 缓冲 */
    uint16_t          payloadLen;
    uint16_t          totalLen;          /* 完整 PDU 长度，含 SID */
    MicroUds_AddrType addrType;
} MicroUds_Request;

typedef struct {
    uint8_t     *buf;        /* 内核 tx 缓冲；buf[0] 预留给 RSID，服务从 buf[1] 起写 */
    uint16_t     capacity;
    uint16_t     length;     /* 服务写入的数据长度（不含 RSID） */
    MicroUds_Nrc nrc;        /* 返回 NRC 时填写 */
} MicroUds_Response;
```

长度约定统一为完整 PDU 字节数（含 SID）。

### 3.4 服务、子功能、数据对象描述符

```c
typedef struct {
    uint8_t               subFunction;
    MicroUds_AccessPolicy access;
    uint16_t              minLen, maxLen;  /* 完整 PDU 长度，0 = 沿用服务级 */
} MicroUds_SubFunctionConfig;       /* 表中不存在 -> 0x12；存在但当前会话不允许 -> 0x7E */

typedef struct {
    MicroUds_Ret (*start)  (struct MicroUds_Ctx *ctx, const MicroUds_Request *req,
                            MicroUds_Response *resp, void *user);
    MicroUds_Ret (*process)(struct MicroUds_Ctx *ctx, MicroUds_Response *resp, void *user);
    void         (*abort)  (struct MicroUds_Ctx *ctx, MicroUds_AbortReason why, void *user);
    void         (*onTxDone)(struct MicroUds_Ctx *ctx, bool ok, void *user);  /* 可选 */
    bool         (*preCondition)(const struct MicroUds_Ctx *ctx, const MicroUds_Request *req); /* 可选，失败 -> 0x22 */
} MicroUds_ServiceOps;

#define MICRO_UDS_SVC_HAS_SUBFUNC     (1u << 0)
#define MICRO_UDS_SVC_ALLOW_SUPPRESS  (1u << 1)
#define MICRO_UDS_SVC_ASYNC           (1u << 2)  /* 可能返回 PENDING */
#define MICRO_UDS_SVC_FUNC_ADDR_OK    (1u << 3)  /* 允许功能寻址 */

typedef struct {
    uint8_t                    sid;
    uint8_t                    flags;
    uint16_t                   minLen, maxLen;
    MicroUds_AccessPolicy      access;
    const MicroUds_SubFunctionConfig *subs;
    uint8_t                    subCount;
    uint32_t                   maxExecMs;   /* 业务期限，0 = 不限（不建议） */
    const MicroUds_ServiceOps *ops;
    void                      *user;
} MicroUds_ServiceConfig;

/* 数据对象：DID */
typedef struct {
    uint16_t              did;
    uint16_t              length;       /* 0 = 变长 */
    MicroUds_AccessPolicy readAccess;
    MicroUds_AccessPolicy writeAccess;  /* sessionMask = 0 即只读 */
    MicroUds_Nrc (*read) (struct MicroUds_Ctx *ctx, uint8_t *out, uint16_t *len, uint16_t cap);
    MicroUds_Nrc (*write)(struct MicroUds_Ctx *ctx, const uint8_t *in, uint16_t len);
} MicroUds_DidConfig;

/* 数据对象：RID（子功能 01 启动 / 02 停止 / 03 结果各自授权） */
typedef struct {
    uint16_t              rid;
    MicroUds_AccessPolicy startAccess, stopAccess, resultAccess;
    uint16_t              minOptLen, maxOptLen;
    MicroUds_Ret (*start) (struct MicroUds_Ctx *ctx, const uint8_t *opt, uint16_t len, MicroUds_Response *resp);
    MicroUds_Ret (*stop)  (struct MicroUds_Ctx *ctx, const uint8_t *opt, uint16_t len, MicroUds_Response *resp);
    MicroUds_Ret (*result)(struct MicroUds_Ctx *ctx, MicroUds_Response *resp);
} MicroUds_RidConfig;
```

服务表和 DID/RID 表建议由生成器按 ID 排序，运行时二分查找；条目少时线性查找即可。

### 3.5 会话表

```c
typedef struct {
    uint8_t  wireId;          /* 线上值：0x01/0x02/0x03/0x04 或 OEM 自定义 */
    uint16_t p2ServerMs;      /* 0x10 正响应中返回 */
    uint16_t p2StarServer10ms;/* 单位 10 ms，对应 0x10 响应的 P2* 字段 */
    uint32_t s3ServerMs;      /* 0 = 本会话无 S3（默认会话） */
    bool     isDefault;
    void   (*onEnter)(struct MicroUds_Ctx *ctx);
    void   (*onExit) (struct MicroUds_Ctx *ctx);
} MicroUds_SessionConfig;
```

会话切换规则：目标会话必须在会话表内；也可为每个会话配置允许迁入的来源掩码，不允许时返回对应 NRC。

### 3.6 安全等级配置与运行时状态

```c
typedef struct {
    uint8_t  requestSeedSub;   /* 奇数 */
    uint8_t  sendKeySub;       /* requestSeedSub + 1 */
    uint8_t  seedLen, keyLen;
    uint8_t  maxAttempts;
    uint32_t lockoutMs;        /* 失败超限后的延时 */
    uint32_t powerOnDelayMs;   /* 上电延时，0 = 无 */
    bool     persistCounter;   /* 失败计数是否写入非易失存储 */
    MicroUds_Nrc (*genSeed)  (struct MicroUds_Ctx *ctx, uint8_t *seed, uint8_t len);
    bool         (*verifyKey)(struct MicroUds_Ctx *ctx, const uint8_t *seed, const uint8_t *key, uint8_t len);
} MicroUds_SecLevelConfig;      /* 表下标即 secMask 位号 */

typedef enum { SEC_IDLE = 0, SEC_SEED_SENT, SEC_LOCKED_OUT } MicroUds_SecPhase;

typedef struct {
    MicroUds_SecPhase phase;
    uint8_t           attempts;
    uint32_t          delayUntil;        /* 延时截止时刻（差值比较） */
    uint8_t           seed[MICRO_UDS_MAX_SEED_LEN];  /* sendKey 后立即清除 */
} MicroUds_SecLevelState;
```

安全访问要点：已解锁等级再请求 seed 时返回全零 seed；sendKey 前没有对应的 requestSeed 返回 0x24；密钥错误返回 0x35，次数超限返回 0x36，延时未到返回 0x37；任何一次 sendKey 之后 seed 都作废。

### 3.7 内核上下文（私有，服务只能通过 API 访问）

```c
typedef enum { REQ_IDLE = 0, REQ_EXECUTING, REQ_PENDING, REQ_WAIT_TX_FINAL } MicroUds_ReqState;
typedef enum { TX_IDLE = 0, TX_BUSY } MicroUds_TxState;

typedef struct MicroUds_Ctx {
    const MicroUds_Config *cfg;

    /* 会话 */
    uint8_t   activeSession;      /* 会话表下标 */
    int16_t   pendingSession;     /* -1 = 无；响应发出后才生效 */

    /* 安全 */
    MicroUds_SecMask       unlocked;
    MicroUds_SecLevelState secState[MICRO_UDS_MAX_SEC_LEVELS];

    /* 请求执行 */
    MicroUds_ReqState  reqState;
    MicroUds_Request   curReq;
    const MicroUds_ServiceConfig *curSvc;
    uint8_t            pendingSent;       /* 已发 0x78 次数 */
    uint8_t            rxBuf[MICRO_UDS_RX_BUF_SIZE];
    uint8_t            txBuf[MICRO_UDS_TX_BUF_SIZE];
    MicroUds_TxState   txState;
    bool               finalIsNegative;

    /* 定时器：全部保存截止时刻 */
    uint32_t p2Deadline, p2StarDeadline, s3Deadline, svcDeadline;
    bool     p2Active, p2StarActive, s3Active, svcActive;
} MicroUds_Ctx;
```

### 3.8 配置与传输适配

```c
typedef struct {
    uint16_t p2ClientMarginMs;   /* 0x78 提前量：在 P2/P2* 到期前多久发出 */
    uint8_t  maxPendingCount;
    bool     busyReturns0x21;    /* 忙时：true 返回 0x21，false 丢弃 */
} MicroUds_Timing;

typedef struct {
    int  (*txPdu)(uint8_t channelId, const uint8_t *data, uint16_t len); /* 异步发送 */
    uint32_t (*getTickMs)(void);                                         /* 单调时基 */
} MicroUds_Port;
/* 传输层发送完成后调用：MicroUds_TxConfirm(ctx, ok) */

typedef struct MicroUds_Config {
    const MicroUds_SessionConfig  *sessions;  uint8_t sessionCount;
    const MicroUds_SecLevelConfig *secLevels; uint8_t secLevelCount;
    const MicroUds_ServiceConfig  *services;  uint8_t serviceCount;
    MicroUds_Timing timing;
    MicroUds_Port   port;
    void (*trace)(const MicroUds_Ctx *ctx, uint8_t event, uint32_t arg);  /* 可选 */
} MicroUds_Config;

/* 时基比较：对回绕安全 */
#define MICRO_UDS_TIME_REACHED(now, deadline)  ((int32_t)((now) - (deadline)) >= 0)
```

## 4. 请求处理流水线

```
RxPdu(入口)
  1. 长度 >= 1？拷贝入 rxBuf；记录寻址类型
  2. 忙状态判断：
       REQ_EXECUTING / PENDING / WAIT_TX -> TesterPresent 可处理；其余按 busyReturns0x21 返回 0x21 或丢弃
  3. 查 SID：未注册 -> 0x11
  4. 服务级会话：已注册但当前会话不允许 -> 0x7F
  5. 功能寻址限制：服务不允许功能寻址 -> 静默
  6. 子功能解析：长度不足以读出子功能 -> 0x13；剥离抑制位（仅 ALLOW_SUPPRESS 服务）
  7. 子功能查找：不在表中 -> 0x12；在表中但当前会话不允许 -> 0x7E
  8. 长度校验：服务/子功能 min/max -> 0x13
  9. preCondition -> 0x22
 10. 安全访问 -> 0x33
 11. 调用 ops->start（DID/RID 级检查在服务内部，返回 0x31 / 0x33）
 12. 按返回值：DONE -> 最终正响应；PENDING -> 启动 P2* 监控并按需发 0x78；NRC -> 负响应
```

要点：上面是推荐的默认顺序，具体以标准和 OEM 规范为准，流水线应作为一个整体由测试固化；统一出口负责功能寻址静默与正响应抑制：成功且抑制位置位时不发正响应，失败时仍按规则发负响应。

## 5. 状态机

### 5.1 会话

默认会话上电进入；0x10 合法切换到非默认会话并启动 S3；任何有效请求刷新 S3；S3 超时或 0x10 切回默认会话时，统一执行：中止运行中服务（abort）、锁定全部安全等级、清空 seed、onExit/onEnter。会话切换在 0x10 正响应发送完成之后才生效（pendingSession）。

### 5.2 请求执行

```
IDLE --收到请求并通过校验--> EXECUTING
EXECUTING --DONE/NRC--> WAIT_TX_FINAL --TxConfirm--> IDLE
EXECUTING --PENDING--> PENDING（发 0x78，进入 P2* 监控）
PENDING --process 返回 DONE/NRC--> WAIT_TX_FINAL
PENDING --P2* 将到期且未超次数--> 再发 0x78
PENDING/EXECUTING --abort 条件--> abort 回调 -> 最终负响应或静默 -> IDLE
```

abort 条件：会话变化、S3 超时、服务期限到、Pending 次数超限、复位。

### 5.3 定时器

| 定时器 | 起点 | 到期动作 |
| --- | --- | --- |
| P2Server | 收到完整请求 | 服务未完成则发 0x78 |
| P2\*Server | 每次发出 0x78 之后 | 仍未完成则再发 0x78（受次数上限约束） |
| S3Server | 响应发送完成 / 收到有效请求 | 回默认会话，执行 5.1 的清理 |
| 服务期限 | start 返回 PENDING | abort(SERVICE\_DEADLINE)，由服务决定最终结果 |

### 5.4 Tx 状态

发送中不能再发起新的发送：最终响应与 0x78 都要排队，等 TxConfirm 后再发；发送失败要有处理策略（重试一次或放弃并复位请求状态）。

## 6. 服务回调契约

- start 只做校验和启动，有副作用的操作必须在全部校验通过之后才提交。
- process 由 MainFunction 周期调用，必须非阻塞，单次执行时间有上限。
- abort 必须可重入安全，且只清理资源，不得发送响应；物理副作用（如已擦除的扇区）由服务自己定义恢复方式。
- 回调内禁止调用 RxPdu；允许调用 SendResponse 辅助函数和内核受控 API。
- 服务不得直接修改 activeSession、unlocked 等私有字段，只能使用：

```c
MicroUds_GetSession(ctx);  MicroUds_RequestSessionChange(ctx, idx);
MicroUds_IsUnlocked(ctx, levelIdx);  MicroUds_LockAll(ctx);
MicroUds_CheckAccess(ctx, &policy);  /* DID/RID 级检查可复用 */
```

## 7. 内置服务要点

- 0x10：校验目标会话在表中；正响应携带 P2 与 P2\*（P2\* 单位为 10 ms）；会话切换在响应发送完成后生效；编程会话通常伴随 bootloader 跳转，应通过 onEnter/onExit 或复位钩子处理。
- 0x27：每个等级一份 SecLevelState；subFunction 与等级映射来自 SecLevelConfig；解锁成功只通过内核 API 置位；会话切换时全部重新锁定。
- 0x3E：支持抑制位；功能寻址的 TesterPresent 常无需响应；任何有效请求都刷新 S3，不仅仅是 0x3E。
- 0x11：正响应必须先发出，再延迟执行复位（利用 onTxDone）。

## 8. 与 MicroRuntime Studio 的配置映射

界面可以按会话分类展示服务，但生成物应是统一的静态表：会话表、安全等级表、服务表、子功能表、DID 表、RID 表，加一份 MicroUds\_Config 和缓冲区宏。生成器的静态检查项：

- SID / DID / RID / 子功能重复；
- 掩码引用的会话或等级下标不存在；
- 服务 maxLen 与 rxBuf 容量、DID 总长与 txBuf 容量冲突；
- 声明 ALLOW\_SUPPRESS 的服务必须 HAS\_SUBFUNC；
- 声明 ASYNC 的服务必须提供 process 与 abort，且 maxExecMs 非 0；
- 默认会话唯一，且 s3ServerMs 为 0。

## 9. 实现顺序与测试清单

1. 入口与流水线：服务注册、SID 查找、长度校验、统一响应出口（含功能寻址静默）。
2. 会话：0x10、会话表、S3、切换副作用、Tx 确认后生效。
3. 安全访问：0x27、等级状态、失败计数与延时、会话切换锁定。
4. 异步：P2/P2\*、0x78、abort、服务期限、忙状态策略。
5. 细分权限与测试：子功能、DID、RID 权限；0x22 多 DID 原子校验。

核心测试用例：未知 SID 的物理/功能寻址；服务在其他会话可用的 0x7F；子功能 0x12 与 0x7E 区分；抑制位成功静默、失败仍返回；NRC 为 0x7F 的 7F xx 7F 报文构造；seed 错序 0x24；密钥错误至超限至延时；会话切换后安全锁定；0x78 期间收到 TesterPresent 与其他请求；S3 超时中止异步服务；响应超缓冲 0x14；时基回绕。

## 10. 待决策清单

| 议题 | 备选 | 建议默认 |
| --- | --- | --- |
| 请求缓冲 | 拷贝 / 零拷贝持有传输缓冲 | 拷贝，简单且异步安全 |
| 忙状态处理 | 0x21 / 丢弃 | 按 OEM 规范配置，默认 0x21 |
| 会话迁移矩阵 | 任意迁移 / 配置矩阵 | 配置矩阵，默认允许 |
| 安全等级继承 | 无继承 / 高等级包含低等级 | 无继承，用掩码显式声明 |
| 失败计数持久化 | RAM / NVM | 量产项目用 NVM |
| 多实例 | 单实例 / 多实例 | 实例化设计，数量由宏裁剪 |
| 注册方式 | 静态表 / 运行时注册 | 静态表为主，运行时注册作为可选 |
