# MicroUds

> 面向单片机的小型、可移植 **UDS（ISO 14229-1）服务端**，C99 编写。
> 只处理**完整的 UDS PDU**——分段、流控由你的 ISO-TP 协议栈（如 MicroIsotp）负责。

[English](readme.md)

- **纯静态内存**：不用 `malloc`，默认配置下 32 位 MCU 约占 2.7 KB RAM。
- **非阻塞、单线程内核**：主循环里调用一个 `TimerHandler()` 就能驱动全部逻辑。
- **与传输层无关**：你提供一个"发送"函数，并在响应真正发完时通知内核即可。
- **声明式服务**：会话、安全等级、寻址规则是表里的**数据**，不是散落在各个回调里的代码。
- **内置** `0x10` `0x27` `0x3E`，以及 **DID 模块**（`0x22` `0x2E` `0x2C`），支持"每个 DID、读写分别"的访问控制。

---

## 目录

1. [当前状态](#1-当前状态)
2. [目录结构](#2-目录结构)
3. [整体关系](#3-整体关系)
4. [快速上手](#4-快速上手)
5. [移植指南](#5-移植指南)
6. [配置项参考](#6-配置项参考)
7. [核心概念](#7-核心概念)
8. [内置服务](#8-内置服务)
9. [编写你自己的服务](#9-编写你自己的服务)
10. [DID 模块（0x22 / 0x2E / 0x2C）](#10-did-模块0x22--0x2e--0x2c)
11. [响应发出之后再做事](#11-响应发出之后再做事)
12. [请求处理流水线与 NRC 速查](#12-请求处理流水线与-nrc-速查)
13. [内存与代码体积](#13-内存与代码体积)
14. [线程/中断规则](#14-线程中断规则)
15. [常见问题排查](#15-常见问题排查)
16. [Client（规划中）](#16-client规划中)
17. [延伸阅读与许可证](#17-延伸阅读与许可证)

---

## 1. 当前状态

| 方面 | 状态 |
|---|---|
| `0x10` 诊断会话控制、`0x27` 安全访问、`0x3E` TesterPresent | 已实现（内置） |
| `0x22` 按标识符读、`0x2E` 按标识符写、`0x2C` 动态定义标识符 | 已实现（DID 模块） |
| 用户自己的服务 + 子功能表 | 已实现 |
| 会话超时（S3）、`0x78` ResponsePending、抑制正响应位、功能寻址规则 | 已实现 |
| 响应发出后的钩子（复位 / 跳转 bootloader） | 已实现 |
| `0x31` RoutineControl、`0x11` ECUReset、`0x14` `0x19` DTC、`0x28`、`0x85`、`0x34`–`0x37` | **未内置**——自己注册（见[第 9 节](#9-编写你自己的服务)和 `example/Server`） |
| Client（诊断仪侧） | **规划中**——见[第 16 节](#16-client规划中) |
| 多个服务端实例 | 不支持（单实例） |
| 大于 31 的 OEM 自定义会话号（如 `0x40`） | 不支持（会话用位掩码，编号 1..31） |
| 变长 DID | 暂不支持（每个 DID 长度固定） |

库在 `-std=c99 -Wall -Wextra -Wpedantic -Wshadow -Wconversion` 下零警告。

---

## 2. 目录结构

```
MicroUds/
├── MicroUds_conf.h        # 配置（改这个）
├── MicroUds_types.h       # 公共类型: 状态码、NRC、会话、服务标志位
├── MicroUds_utils.h       # 辅助宏（会话掩码、权限字节、tick 运算）
├── CMakeLists.txt
├── Server/
│   ├── include/
│   │   ├── MicroUdsServer.h            # 对外 API  <- 包含这个
│   │   ├── MicroUdsServer_types.h      # 服务表、回调上下文、内核状态
│   │   ├── MicroUdsServer_Did.h        # DID 模块 API
│   │   └── MicroUdsServer_Did_types.h  # DID 表与 DID 回调上下文
│   └── src/
│       ├── MicroUdsServer.c
│       └── MicroUdsServer_Did.c
├── Client/                # 预留（尚未实现）
├── example/
│   ├── Server/            # 可在 PC 上直接运行的示例  <- 从这里开始
│   └── Client/            # 预留
└── doc/                   # 设计笔记（中文）
```

---

## 3. 整体关系

```
 你的应用          服务表、DID 表、安全算法、钩子
        │  ▲
        ▼  │   MicroUdsServer_ServiceRegisterServ / _Did_RegisterReadItem / 各种回调
 ┌────────────────────────────────────────────────────────────┐
 │ MicroUds Server                                            │
 │   分发流水线 · 会话 · 安全 · S3 · 0x78                      │
 │   响应队列 · 发送确认 · DID 模块                            │
 └────────────────────────────────────────────────────────────┘
        │  ▲
        ▼  │   RespTransmit()  /  RxIndication()  /  HwRespTxDone()
 ISO-TP 协议栈（MicroIsotp 或其他） ── CAN 驱动
```

谁调用谁：

| 函数 | 谁调用 | 在哪里调用 |
|---|---|---|
| `MicroUdsServer_Init()` | 你 | 启动时一次，**必须先于任何注册** |
| `MicroUdsServer_TickHandler()` | 你 | 定时中断，频率 `MICROUDS_FREQ_HZ` |
| `MicroUdsServer_TimerHandler()` | 你 | 主循环 / 任务（不要在中断里调） |
| `MicroUdsServer_RxIndication()` | ISO-TP | "收到完整报文"回调（中断或任务） |
| `MicroUdsServer_HwRespTxDone()` | ISO-TP | "报文发送完成"回调（中断或任务） |
| `MicroUdsServer_RespTransmit()` | 内核 | **由你实现**——把 PDU 交给 ISO-TP |

---

## 4. 快速上手

### 4.1 把库加入工程

**CMake**

```cmake
add_library(microuds STATIC
    MicroUds/Server/src/MicroUdsServer.c
    MicroUds/Server/src/MicroUdsServer_Did.c)

target_include_directories(microuds PUBLIC
    MicroUds                      # MicroUds_conf.h、MicroUds_types.h、MicroUds_utils.h
    MicroUds/Server/include)      # MicroUdsServer*.h

target_link_libraries(your_app PRIVATE microuds)
```

**Keil / IAR / Makefile**：加入上面两个 `.c`，并添加两个头文件目录。需要 C99、`stdint.h`、`stdbool.h`，以及支持 `__attribute__((weak))` 的编译器。

> **弱符号。** `RespTransmit`、`SecurityReqSeed`、`SecurityGetKey`、`OnResponseSent`、`Did_AddrReadable` 都是带无害默认实现的弱函数，请在你的**应用源码**里定义自己的版本。如果你把 MicroUds 编成**静态库**，不要把重写的函数放进另一个没人引用的静态库里——链接器不会拉入那个目标文件，弱默认实现会悄悄生效。

### 4.2 最小接入

```c
#include "MicroUdsServer.h"

/* 1. 发送: 把响应交给 ISO-TP。不能阻塞。
 *    isotp_is_idle() / isotp_send() 只是占位，换成你自己的 ISO-TP 接口。 */
MicroUds_Status_t MicroUdsServer_RespTransmit(const uint8_t *pdu, uint32_t len)
{
    if (!isotp_is_idle())            return MICROUDS_BUSY;   /* 内核下个周期重试 */
    isotp_send(pdu, len);
    return MICROUDS_OK;
}

/* 2. ISO-TP 回调 */
void isotp_on_message_sent(void)                     { MicroUdsServer_HwRespTxDone(); }
void isotp_on_message_received(const uint8_t *d, size_t n, bool functional)
{
    MicroUdsServer_RxIndication(d, n, functional ? MICROUDS_ADDR_FUNC : MICROUDS_ADDR_PHY);
}

/* 3. 心跳: 频率 MICROUDS_FREQ_HZ（默认 1 kHz） */
void SysTick_Handler(void) { MicroUdsServer_TickHandler(); }

int main(void)
{
    MicroUdsServer_Init();                                   /* 永远最先 */
    MicroUdsServer_ServiceRegisterServ(my_services, MICROUDS_SIZEOF(my_services));
    MicroUdsServer_Did_RegisterReadItem(my_read_dids, MICROUDS_SIZEOF(my_read_dids));
    MicroUdsServer_Did_RegisterWriteItem(my_write_dids, MICROUDS_SIZEOF(my_write_dids));

    for (;;)
    {
        MicroUdsServer_TimerHandler();                       /* 驱动一切 */
        /* ... 你主循环里的其他事情 ... */
    }
}
```

这样就能响应 `10 01/02/03`、`3E 00`、`27 xx`（实现两个安全函数之后，见 [8.2](#82-0x27-安全访问)），以及你注册的所有 DID。

### 4.3 运行示例（不需要硬件）

`example/Server/main.c` 在 PC 上模拟了一个 ISO-TP，依次发送一系列请求并打印每条响应。

```bash
cd example/Server
cmake -S . -B build
cmake --build build
./build/server_example
```

不用 CMake 也可以（在仓库根目录执行）：

```bash
gcc -std=c99 -I. -IServer/include \
    example/Server/main.c Server/src/MicroUdsServer.c Server/src/MicroUdsServer_Did.c \
    -o server_example
```

输出节选：

```
# enter extended session
> 10 03
< 50 03 00 32 01 F4

# read serial + live temperature (two DIDs in one request)
> 22 F1 8C 02 00
< 62 F1 8C 00 00 12 34 02 00 00 FB

# slow write (answers 0x78 first, then the final response)
> 2E 07 00 CA FE BA BE
< 7F 2E 78
< 6E 07 00

# ECUReset hard (the reset happens AFTER the response)
> 11 01
< 51 01
  [hook] 11 01 answered -> ECU hard reset (re-initialising)
```

示例分六节（移植层、安全算法、DID 表、用户服务、响应钩子、演示驱动）。上真实硬件时保留第 2–5 节，替换第 1 节和第 6 节。

---

## 5. 移植指南

### 你要调用的函数

| 函数 | 说明 |
|---|---|
| `MicroUds_Status_t MicroUdsServer_Init(void)` | 重置内核并注册内置服务。之前注册的所有内容都会被丢弃。 |
| `void MicroUdsServer_TickHandler(void)` | 内部 tick 加一。必须严格按 `MICROUDS_FREQ_HZ` 调用。 |
| `void MicroUdsServer_TimerHandler(void)` | 内核主函数。每次调用会推进 S3 计时，最多分发一个请求，并最多推进一步响应发送。建议每 1 ms 调一次，至少每几毫秒一次（默认对外声明的 P2 是 50 ms）。 |
| `MicroUds_Status_t MicroUdsServer_RxIndication(const uint8_t *data, size_t len, MicroUds_AddrType_t addr)` | 送入一条**完整**请求 PDU（首字节是 SID）。数据会被拷贝，你的缓冲可以立即复用。返回 `MICROUDS_OK`；`MICROUDS_BUSY` 表示上一个请求还没处理完（包括"正在等它的响应发完"），本条被丢弃，是否重试或回 `0x21` 由你决定；`MICROUDS_PARAM_ERR` 表示空指针或长度为 0。 |
| `void MicroUdsServer_HwRespTxDone(void)` | 通知内核：交给 `RespTransmit` 的那条响应已经发完（或发送失败）。只置一个标志，可以在中断里调用。 |
| `uint8_t MicroUdsServer_GetSession(void)` | 当前会话编号。 |
| `uint8_t MicroUdsServer_GetSecurityLevel(void)` | 当前已解锁的等级，`0` = 未解锁。 |

`addr` 取 `MICROUDS_ADDR_PHY`（物理寻址）或 `MICROUDS_ADDR_FUNC`（功能寻址）。内核需要它，因为两者的响应规则不同（见 [7.4](#74-物理寻址与功能寻址)）。

### 你要实现的函数（弱函数）

| 函数 | 是否必须 | 用途 |
|---|---|---|
| `MicroUds_Status_t MicroUdsServer_RespTransmit(const uint8_t *pdu, uint32_t len)` | **必须** | 把响应交给 ISO-TP。接受则返回 `MICROUDS_OK`（报文真正发完后调用 `HwRespTxDone`）；传输层暂时忙返回 `MICROUDS_BUSY`（内核下个周期重试）；其他值表示丢弃这条响应。默认实现什么都不做并返回 OK，所以**忘了实现只会静默，不会报错**。 |
| `void MicroUdsServer_SecurityReqSeed(uint8_t level, uint8_t *out, uint32_t *len)` | 用 `0x27` 时 | 生成种子。进入时 `*len` 是缓冲容量（`MICROUDS_SEED_SIZE`），返回时设为实际写入的字节数；设为 `0` 表示失败。默认实现设为 `0`，所以实现它之前 `27 01` 会回 `0x10`。 |
| `void MicroUdsServer_SecurityGetKey(uint8_t level, const uint8_t *seed, uint32_t seed_len, uint8_t *key, uint32_t *key_len)` | 用 `0x27` 时 | 计算诊断仪对这个种子应该发来的密钥。进入时 `*key_len` 是容量（`MICROUDS_KEY_SIZE`），返回时设为密钥长度。默认实现设为 `0`，所以任何密钥都会被拒绝（安全的默认值）。 |
| `void MicroUdsServer_OnResponseSent(MicroUdsServer_ServiceCtx_t *ctx)` | 可选 | 见[第 11 节](#11-响应发出之后再做事)。 |
| `bool MicroUdsServer_Did_AddrReadable(uint32_t addr, uint32_t len)` | 仅当 `MICROUDS_DID_DYNA_BY_ADDR = 1` | `0x2C 02` 的地址白名单。默认全部拒绝。 |

种子必须来自硬件随机数，密钥算法应使用真正的 MAC（如 AES-CMAC）。示例里的异或只用于演示。

---

## 6. 配置项参考

修改 `MicroUds_conf.h`。时间单位是毫秒，内部用 `MICROUDS_MS_TO_TICK()` 换算。

| 宏 | 默认值 | 含义 |
|---|---|---|
| `MICROUDS_SERVER_ENABLE` | `1` | 编译服务端。 |
| `MICROUDS_CLIENT_ENABLE` | `0` | 为 Client 预留。 |
| `MICROUDS_USE_MEM` | `0` | 预留（可选的 MicroMem 集成），保持 `0`。 |
| `MICROUDS_FREQ_HZ` | `1000` | tick 频率，必须与你调用 `TickHandler` 的频率一致。 |
| `MICROUDS_SESSION_HOLD_MS` | `5000` | S3：非默认会话下空闲多久后回到默认会话。 |
| `MICROUDS_P2_SERVER_MS` | `50` | `0x10` 响应里报告的 P2。 |
| `MICROUDS_P2STAR_SERVER_MS` | `5000` | `0x10` 响应里报告的 P2\*。 |
| `MICROUDS_PENDING_RESEND_MS` | `4000` | 服务处于 pending 时，每隔多久重发一次 `0x78`。必须小于 `P2STAR`。 |
| `MICROUDS_PENDING_MAX_COUNT` | `10` | `0x78` 最多发几次，超过则内核放弃并回 `0x10`。 |
| `MICROUDS_TX_CONFIRM_TIMEOUT_MS` | `1000` | `RespTransmit` 返回 OK 之后，最多等 `HwRespTxDone` 多久；超时就认为这条响应已结束。 |
| `MICROUDS_REQUEST_BUF_SIZE` | `64` | 最大请求长度（完整 PDU，字节）。更长的请求回 `0x13`。 |
| `MICROUDS_RESPONSE_BUF_SIZE` | `64` | 最大响应长度（完整 PDU，字节）。 |
| `MICROUDS_SESSION_MASK` | `4` | 支持的**最大**会话编号：`4` 表示支持会话 1、2、3、4。 |
| `MICROUDS_SEED_SIZE` / `MICROUDS_KEY_SIZE` | `4` / `4` | 安全访问的种子/密钥长度。 |
| `MICROUDS_SECURITY_MAX_LEVEL` | `4` | 安全等级数量（`27 01/02` = 等级1 ……）。 |
| `MICROUDS_SECURITY_MAX_ATTEMPTS` | `3` | 连续失败多少次后锁定。 |
| `MICROUDS_SECURITY_DELAY_MS` | `10000` | 失败过多后的锁定时间。 |
| `MICROUDS_SERVICE_MAX_NUMBER` | `16` | 服务表容量。**内置服务占 3 个（带 DID 模块占 6 个）**，留给你 10 个。 |
| `MICROUDS_SUBFUNC_MAX_NUMBER` | `3` | 每个服务表项的子功能数量。 |
| `MICROUDS_DID_ENABLE` | `1` | 编译 DID 模块。 |
| `MICROUDS_DID_READ_SIZE` / `MICROUDS_DID_WRITE_SIZE` | `8` / `8` | 读表 / 写表容量。 |
| `MICROUDS_DID_DYNAMICALLY_SIZE` | `8` | 动态 DID 个数。 |
| `MICROUDS_DID_DYNAMICALLY_ITEM_LEN` | `5` | 每个动态 DID 的片段数。 |
| `MICROUDS_DID_READ_MAX_PER_REQ` | `8` | 一条 `0x22` 最多带几个 DID，超过回 `0x31`。 |
| `MICROUDS_DID_READ_PARTIAL` | `0` | `0` = `0x22` 里只要有一个 DID 不存在/不可用，整条失败；`1` = 跳过这些 DID，回答其余的。 |
| `MICROUDS_DID_NRC_SESSION` | `0x31` | DID 存在但当前会话不可用时返回的 NRC。 |
| `MICROUDS_DID_DYNA_ID_MIN` / `_MAX` | `0xF300` / `0xF3FF` | 允许作为动态 DID 的范围。 |
| `MICROUDS_DID_DYNA_BY_ADDR` | `1` | 允许 `0x2C 02`（按内存地址定义动态 DID）。**会把内存暴露给诊断仪——白名单务必收紧，或设为 `0`。** |
| `MICROUDS_DID_DYNA_CLEAR_ON_DEFAULT` | `1` | 回到默认会话时删除所有动态 DID。 |
| `MICROUDS_DID_READ_REQ_MIN_LEN` / `WRITE_REQ_MIN_LEN` / `DYNA_REQ_MIN_LEN` | `3` / `4` / `2` | 内置 DID 服务的请求最小长度，一般不用改。 |
| `MICROUDS_DID_RESP_MAX_LEN` / `RESP_MIN_LEN` | `32` / `2` | 未使用（为兼容保留）。 |

---

## 7. 核心概念

### 7.1 一次只处理一个请求

内核只有一份请求缓冲和一份响应缓冲。从 `RxIndication` 开始，直到它的**最终响应发送完成**（`HwRespTxDone`），请求都处于"处理中"，这期间新来的请求会得到 `MICROUDS_BUSY`。`0x78` ResponsePending 不是最终响应：它发完后请求仍然存活。

### 7.2 会话

会话编号就是 `10 xx` 里的 `xx`：

| 常量 | 值 | 典型用途 |
|---|---|---|
| `MICROUDS_SESSION_DEFAULT` | 1 | 上电默认会话 |
| `MICROUDS_SESSION_PROGRAM` | 2 | 编程 |
| `MICROUDS_SESSION_EXTEND` | 3 | 扩展诊断 |
| `MICROUDS_SESSION_SECURITY` | 4 | 安全系统诊断 |

配置里写"会话掩码"的地方，每个会话占一位：

```c
uint32_t mask = MICROUDS_SET_SESSION(MICROUDS_SESSION_DEFAULT) |
                MICROUDS_SET_SESSION(MICROUDS_SESSION_EXTEND);
```

内核实现的规则：

- 上电进入默认会话。`10 xx` 先回响应，**响应发出之后**才切换会话。
- 会话真正发生变化时，**安全重新锁定**（失败计数不清零）；开启 `MICROUDS_DID_DYNA_CLEAR_ON_DEFAULT` 时，回到默认会话还会删除动态 DID。
- 请求切到"当前已在的会话"不会锁定安全（各 OEM 规范不一致；如果你的规范不同，修改 `MicroUdsServer_SessionSwitch`）。
- **S3**：在非默认会话下，`MICROUDS_SESSION_HOLD_MS` 内没有任何动作，就回到默认会话。收到请求和请求结束时都会重新计时，请求处理期间不计时。
- P2/P2\* 只是在 `0x10` 响应里**对外声明**，内核不会对 P2 计时。凡是可能超过 P2 的操作，请返回 **pending**（9.3 节）。

### 7.3 安全等级与权限字节

`27 01/02` 解锁等级 1，`27 03/04` 解锁等级 2，依此类推。**同一时刻只有一个等级有效**：解锁另一个等级会替换当前等级。

*权限字节*（`permis`，用在服务表、子功能表、DID 表里）规定哪些已解锁等级可以使用这一项：

| 取值 | 辅助宏 | 含义 |
|---|---|---|
| `0` | — | 无需解锁 |
| `n` | `MICROUDS_PERMIS_ATLEAST(n)` | 当前等级必须 **≥ n** |
| `0x80 \| n` | `MICROUDS_PERMIS_EXACT(n)` | 当前等级必须**恰好等于 n** |

### 7.4 物理寻址与功能寻址

- 服务只有带 `MICROUDS_SERV_FLAG_FUNC_ADDR` 才接受功能寻址请求，否则这条请求被静默忽略。
- 内置服务：`0x10`、`0x3E` 接受功能寻址；`0x27`、`0x22`、`0x2E`、`0x2C` 不接受。
- 对功能寻址请求，NRC 为 `0x11 0x12 0x31 0x7E 0x7F` 的负响应**不发送**（避免广播请求让所有 ECU 都回"不支持"）。正响应照常发送。

### 7.5 抑制正响应

服务带 `MICROUDS_SERV_FLAG_SUPPRESS`，并且请求的子功能 bit7 为 1（`3E 80`、`10 83`、`11 81` ……）时，不发送正响应。负响应始终发送。服务照常执行，`OnResponseSent` 也照常调用。这类服务的子功能编号必须小于 `0x80`。

---

## 8. 内置服务

| 服务 | 会话 | 功能寻址 | 抑制位 | 请求长度 | 行为 |
|---|---|---|---|---|---|
| `0x10` | 所有支持的会话 | 是 | 是 | 恰好 2 | 正响应：`50 <会话> <P2 高 低> <P2* 高 低>`（P2\* 单位 10 ms）。不支持的会话 → `0x12`。 |
| `0x27` | 编程、扩展 | 否 | 否 | ≥ 2 | 见下文。 |
| `0x3E` | 所有支持的会话 | 是 | 是 | 恰好 2 | 只支持子功能 `00`，其他 → `0x12`。 |
| `0x22` `0x2E` `0x2C` | 见[第 10 节](#10-did-模块0x22--0x2e--0x2c) | | | | DID 模块 |

### 8.1 `0x10` 诊断会话控制

无需配置。支持的会话是 `1 … MICROUDS_SESSION_MASK`。

### 8.2 `0x27` 安全访问

- 奇数子功能 = 请求种子，下一个偶数 = 发送密钥（等级 1 是 `27 01` / `27 02`）。
- 合法子功能：`1 … 2 × MICROUDS_SECURITY_MAX_LEVEL`，否则 `0x12`。
- 请求种子：延时计时器在跑 → `0x37`；该等级已解锁 → 回全 0 种子；否则调用你的 `SecurityReqSeed`（若报告长度为 0 → `0x10`）。
- 发送密钥必须紧跟在**同一等级**的请求种子之后，否则 `0x24`。密钥长度必须是 `MICROUDS_KEY_SIZE`，否则 `0x13`（种子仍然有效）。期望密钥由你的 `SecurityGetKey` 算出，密钥错误 → `0x35`。
- 种子只能用一次：任何一次发送密钥之后，无论成败，种子都作废。
- 连续失败 `MICROUDS_SECURITY_MAX_ATTEMPTS` 次后回 `0x36`，并且在 `MICROUDS_SECURITY_DELAY_MS` 内请求种子回 `0x37`。切换会话和 S3 超时**不会**清零失败计数。

---

## 9. 编写你自己的服务

### 9.1 服务表

```c
static const MicroUdsServer_ServiceTable_t my_services[] = {
    {
        .sid          = 0x11,                                          /* 服务 ID */
        .flags        = MICROUDS_SERV_FLAG_HAS_SUBFUNC |               /* 第 2 字节是子功能 */
                        MICROUDS_SERV_FLAG_SUPPRESS,                   /* 子功能 bit7 = 抑制位 */
        .permis       = 0,                                             /* 安全要求 (7.3) */
        .session_mask = MICROUDS_SET_SESSION(MICROUDS_SESSION_EXTEND) |
                        MICROUDS_SET_SESSION(MICROUDS_SESSION_PROGRAM),
        .min_len      = 2,                                             /* 完整 PDU 长度（含 SID），0 = 不检查 */
        .max_len      = 2,
        .callback     = svc_ecu_reset,
        .Userdata     = NULL,
    },
};

MicroUdsServer_ServiceRegisterServ(my_services, MICROUDS_SIZEOF(my_services));   /* 在 Init 之后 */
```

| 字段 | 含义 |
|---|---|
| `sid` | 服务 ID。不能与内置服务或其他表项重复。 |
| `flags` | `HAS_SUBFUNC`：请求第 2 字节是子功能——内核负责解析、处理抑制位，并在**正响应里回显子功能**。`SUPPRESS`：子功能 bit7 是抑制位（必须同时有 `HAS_SUBFUNC`）。`FUNC_ADDR`：接受功能寻址。不要设置 `BUILTIN`。 |
| `permis` | 整个服务的安全要求。 |
| `session_mask` | 服务存在的会话。其他会话 → `0x7F`。取 `0` 表示"任何会话都不可用"（`0x11`）。 |
| `min_len` / `max_len` | 完整 PDU 长度，含 SID。不满足 → `0x13`。 |
| `callback` | 请求通过全部检查后调用。只有在你注册了子功能表时才可以为 `NULL`。 |
| `Userdata` | 原样作为 `ctx->Userdata` 传回。 |

`MicroUdsServer_ServiceRegisterServ` 是**原子的**（任何一项非法则一项都不注册），可以多次调用。以下情况返回 `MICROUDS_PARAM_ERR`：空表/空指针、服务表已满、SID 重复、设置了 `BUILTIN`、`SUPPRESS` 却没有 `HAS_SUBFUNC`。

### 9.2 回调上下文

```c
static void svc_ecu_reset(MicroUdsServer_ServiceCtx_t *ctx)
{
    if (ctx->sub_func == 0x01)
    {
        MicroUdsServer_CtxPositive(ctx, 0);                 /* 回 51 01 */
        return;
    }
    MicroUdsServer_CtxNegative(ctx, MICROUDS_NRC_SFNS);     /* 回 7F 11 12 */
}
```

**每次都必须给出结果。** 上下文初始是"负响应，`0x10`"；请恰好调用下面之一：

| 辅助函数 | 结果 |
|---|---|
| `MicroUdsServer_CtxPositive(ctx, len)` | 正响应。你写进 `ctx->payload` 的 `len` 字节会接在 SID（和子功能）之后。 |
| `MicroUdsServer_CtxNegative(ctx, nrc)` | 负响应 `7F <sid> <nrc>`。 |
| `MicroUdsServer_CtxPending(ctx)` | 还没做完——见 9.3。 |

输入（只读）：

| 字段 | 含义 |
|---|---|
| `sid`、`sub_func` | 服务 ID 与子功能（已去掉抑制位；服务没有子功能时为 `0`）。 |
| `req`、`req_len` | SID **之后**的请求字节（如果有 `HAS_SUBFUNC`，则是子功能之后）。 |
| `addr` | `MICROUDS_ADDR_PHY` 或 `_FUNC`。 |
| `session`、`sec_level` | 当前会话编号与已解锁等级。 |
| `call_cnt` | 第一次调用为 `0`，*pending* 之后每次重入依次为 1、2、…… |
| `Userdata` | 来自表项。 |

输出：`payload` 指向响应缓冲（已越过 SID/子功能），`payload_cap` 是剩余空间；往里写，然后用长度调用 `CtxPositive`。放不下时内核回 `0x14`。

> 你的回调运行在 `TimerHandler` 里。请保持简短、不要阻塞。

### 9.3 耗时操作：*pending*

```c
static void rc_erase(MicroUdsServer_ServiceCtx_t *ctx)
{
    if (!flash_erase_done())
    {
        MicroUdsServer_CtxPending(ctx);          /* 内核立刻发 7F <sid> 78 */
        return;                                  /* ……并且每个周期再调用本函数 */
    }
    MicroUdsServer_CtxPositive(ctx, 0);          /* 最终: 真正的响应 */
}
```

- 第一次 `CtxPending` 会立即发 `0x78`。之后每次 `TimerHandler` 都会再次调用该回调（`ctx->call_cnt` 递增），直到它返回正响应或负响应。
- 在 `call_cnt == 0` 时启动真正的工作；状态请保存在你自己的变量里（每次调用上下文都会重新构建）。
- `0x78` 每隔 `MICROUDS_PENDING_RESEND_MS` 重发一次；重发 `MICROUDS_PENDING_MAX_COUNT` 次之后，内核以 `0x10` 结束请求。
- pending 期间新请求得到 `BUSY`。

### 9.4 子功能表

如果每个子功能有各自的处理函数或各自的权限，就注册一张表。把服务表项的 `.callback` 设为 `NULL`：注册了子功能表之后，服务级回调不会再被使用。

```c
static const MicroUdsServer_SubFuncTable_t rc_subs[] = {
    { .id = 0x01, .callback = rc_start,   .permis = MICROUDS_PERMIS_ATLEAST(1) },  /* 需要等级 1 */
    { .id = 0x02, .callback = rc_stop    },
    { .id = 0x03, .callback = rc_results },
};
MicroUdsServer_ServiceRegisterSubFunc(0x31, rc_subs, MICROUDS_SIZEOF(rc_subs));
```

| 字段 | 含义 |
|---|---|
| `id` | 子功能编号（服务带 `SUPPRESS` 时必须小于 `0x80`）。 |
| `callback` | 这个子功能的处理函数。**不能为 `NULL`**（`NULL` 会回 `0x10`，不会回退到服务级回调）。 |
| `Userdata` | 作为 `ctx->Userdata` 传入。 |
| `session_mask` | `0` = 与服务相同；否则是该子功能存在的会话。存在但当前会话不允许 → `0x7E`。 |
| `permis` | 这个子功能**额外**的安全要求。 |

注册了表之后，不在表里的子功能回 `0x12`。再次注册会替换整张表。只有带 `HAS_SUBFUNC` 的用户服务才能有子功能表（最多 `MICROUDS_SUBFUNC_MAX_NUMBER` 项）。

不注册表时，由一个服务级回调通过 `ctx->sub_func` 处理全部子功能（内置服务就是这样工作的）。

`example/Server/main.c` 里有一个用这种方式写的完整 `0x31` RoutineControl，包括使用 *pending* 的耗时例程。

---

## 10. DID 模块（0x22 / 0x2E / 0x2C）

### 10.1 定义 DID

读表和写表是分开的。**不在写表里的 DID 就是只读。** 同一个 DID 可以同时出现在两张表里，权限各不相同。

```c
static uint8_t g_vin[17] = { 'M','I','C','R','O','U','D','S','0','0','0','0','0','0','0','0','1' };
static uint8_t g_cfg[2];

static const MicroUdsServerDidItem_Config_t read_dids[] = {
    { .Identifier = 0xF190, .data = g_vin, .data_len = sizeof g_vin },                 /* 所有会话，无需解锁 */
    { .Identifier = 0x0100, .data = g_cfg, .data_len = sizeof g_cfg,
      .session = MICROUDS_SET_SESSION(MICROUDS_SESSION_EXTEND) },                      /* 仅扩展会话 */
};
static const MicroUdsServerDidItem_Config_t write_dids[] = {
    { .Identifier = 0x0100, .data = g_cfg, .data_len = sizeof g_cfg,
      .session = MICROUDS_SET_SESSION(MICROUDS_SESSION_EXTEND),
      .permis  = MICROUDS_PERMIS_ATLEAST(1) },                                         /* 扩展会话 + 已解锁 */
};

MicroUdsServer_Did_RegisterReadItem (read_dids,  MICROUDS_SIZEOF(read_dids));   /* 在 Init 之后 */
MicroUdsServer_Did_RegisterWriteItem(write_dids, MICROUDS_SIZEOF(write_dids));
```

| 字段 | 含义 |
|---|---|
| `Identifier` | 16 位 DID。 |
| `session` | DID 可用的会话掩码。**`0` = 所有会话。** |
| `permis` | 安全要求，见 7.3。**`0` = 无需解锁。** |
| `data` / `data_len` | 你的变量及其大小。DID 是**固定长度**。 |
| `cb` | 可选回调，见 10.2。 |
| `Userdata` | 作为 `ctx->Userdata` 传给回调。 |

> `session = 0` 和 `permis = 0` 都表示*没有限制*。漏配一个字段就会把 DID 放开，凡是敏感的 DID 请把两项都显式写出来。

注册函数可以多次调用（表会追加），并且是原子的。以下情况返回 `MICROUDS_PARAM_ERR`：空表、没有空位、DID 重复、`data_len == 0`、**读**表项的 `data == NULL`、读表项长度超过 `MICROUDS_RESPONSE_BUF_SIZE − 3`、**写**表项既没有 `data` 也没有 `cb`、写表项在请求缓冲里放不下。

访问检查是：*服务级*条件 **且** DID 自己的 `session`/`permis`。内置的 `0x22`/`0x2E` 在默认、编程、扩展会话（不含会话 4）下可用且无需解锁，所以真正的策略写在每个 DID 上。

### 10.2 DID 回调

回调在内核拷贝任何数据**之前**调用。

```c
typedef struct {
    /* 输入 */
    uint16_t Did;  uint16_t Did_len;          /* 这个 DID；本次请求里 DID 的个数 */
    uint8_t *data; uint16_t data_len;         /* 你的变量（只写 DID 可能是 NULL） */
    void *Userdata;
    bool is_write;
    const uint8_t *in_data; uint16_t in_len;  /* 写: 诊断仪发来的字节（只读） */
    uint8_t session, sec_level;
    uint32_t call_cnt;                        /* 写 + pending: 首次 0，之后 1、2 …… */
    /* 输出 */
    uint8_t nrc;                              /* 置位表示拒绝 */
    bool pending;                             /* 仅写: 还没做完 */
} MicroUdsServerDid_Ctx_t;
```

- **读**：把 `ctx->data` 刷新成当前值（比如传感器读数）。随后内核把 `data` 拷进响应。设置 `ctx->nrc` 表示拒绝。读不支持 pending（会回 `0x10`）。
- **写**：校验 `ctx->in_data`（范围、状态……）。如果保持 `nrc == 0`，写入就被**接受**，内核会把 `in_data` 拷进 `data`（`data` 非 `NULL` 时）。如果设置了 `nrc`，什么都不会改。如果你的变量不是普通 RAM（如 NVM），把 `data` 设为 `NULL`，在回调里自己保存这些字节。
- **慢速写入**：设置 `ctx->pending = true`。内核回 `0x78`，之后每个周期再调用回调（`call_cnt` 递增），回调不再设置 `pending` 时写入才被接受。

### 10.3 `0x22` 按标识符读

`22 <DID> [<DID> ...]` → `62 <DID> <数据> [<DID> <数据> ...]`

- 请求长度必须是 `1 + 2N`（否则 `0x13`），且 `N ≤ MICROUDS_DID_READ_MAX_PER_REQ`（否则 `0x31`）。
- **两阶段**：先检查每个 DID（是否存在、会话、安全、总长度），*不读取任何数据*；然后才读数据。所以有问题的 DID 不会留下执行了一半的读回调。
- DID 不存在 → `0x31`。DID 存在但当前会话不可用 → `MICROUDS_DID_NRC_SESSION`（默认 `0x31`）。开启 `MICROUDS_DID_READ_PARTIAL = 1` 时跳过这些 DID、回答其余的（什么都不剩才回 `0x31`）。
- **任何一个** DID 安全不满足 → 整条请求回 `0x33`，绝不返回部分数据。
- 响应超过 `MICROUDS_RESPONSE_BUF_SIZE` → `0x14`。
- 静态 DID 和动态 DID 可以在同一条请求里混用。

### 10.4 `0x2E` 按标识符写

`2E <DID> <数据>` → `6E <DID>`

- DID 必须在写表里，否则 `0x31`（这也保证动态 DID 永远不能被写）。
- 检查顺序：DID 存在 → 会话/安全（`0x31` / `0x33`）→ 长度必须严格等于 `data_len`（`0x13`）→ 你的回调。

### 10.5 `0x2C` 动态定义标识符

让诊断仪用已有 DID 的片段拼出一个新的 DID。在编程会话和扩展会话中可用。

| 子功能 | 请求 | 说明 |
|---|---|---|
| `01` 按标识符定义 | `2C 01 <DDDID> { <源DID> <起始位置> <长度> } × N` | 起始位置**从 1 开始**。最多 `MICROUDS_DID_DYNAMICALLY_ITEM_LEN` 个片段。 |
| `02` 按地址定义 | `2C 02 <DDDID> <ALFID> { <地址> <长度> } × N` | 仅当 `MICROUDS_DID_DYNA_BY_ADDR = 1`。ALFID 必须是 `0x14` 或 `0x24`（**4 字节地址**，1 或 2 字节长度），否则 `0x31`。开关关闭时该子功能回 `0x12`。 |
| `03` 清除 | `2C 03` 或 `2C 03 <DDDID>` | 不带 DID：清除全部。DID 未定义 → `0x31`。 |

正响应：`6C <子功能> <DDDID>`（清除全部时是 `6C 03`）。

规则：

- `DDDID` 必须在 `MICROUDS_DID_DYNA_ID_MIN … MAX` 之内，并且不能是静态 DID。
- 每个源 DID 在*此刻*必须可读（会话 + 安全）。这个检查在**每次读取动态 DID 时都会重做**，所以动态 DID 永远不能用来绕过源 DID 的保护。
- 总长度必须能放进响应缓冲。对已存在的 `DDDID` 再次定义会**替换**它。定义失败不会改变任何东西。
- 回到默认会话时删除所有动态 DID（`MICROUDS_DID_DYNA_CLEAR_ON_DEFAULT`）。
- 子功能 `02` 读取的是**原始内存**。唯一的保护是你的 `MicroUdsServer_Did_AddrReadable()` 白名单（默认全部拒绝）以及"必须在编程/扩展会话"；当前实现不要求安全解锁。如果不需要，请把 `MICROUDS_DID_DYNA_BY_ADDR` 设为 `0`。

```c
bool MicroUdsServer_Did_AddrReadable(uint32_t addr, uint32_t len)
{
    return (addr >= 0x20000000u) && (len <= 0x100u) && ((addr - 0x20000000u) <= (0x100u - len));
}
```

---

## 11. 响应发出之后再做事

复位 ECU、跳转 bootloader 必须在正响应已经发到总线上**之后**再执行。实现一个钩子，所有服务（内置的和你的）共用：

```c
void MicroUdsServer_OnResponseSent(MicroUdsServer_ServiceCtx_t *ctx)
{
    if (ctx->resp_type != MICROUDS_RESPTYPE_POSITIVE)      /* 重要: 负响应也会进到这里 */
    {
        return;
    }
    if (ctx->sid == 0x11 && ctx->sub_func == 0x01) { NVIC_SystemReset(); }
    if (ctx->sid == 0x10 && ctx->sub_func == 0x02) { jump_to_bootloader(); }
}
```

什么时候会被调用：

| 情形 | 是否调用钩子 |
|---|---|
| 某个服务回调执行了，并且它的最终响应（正响应**或负响应**）已发送 | 是 |
| 正响应被抑制（`11 81`） | 是——复位仍然要执行 |
| `0x78` ResponsePending | 否 |
| 请求在到达服务之前就被流水线拒绝（未知 SID、会话不对、长度错误、安全不满足……） | 否 |
| 被静默忽略的功能寻址请求 | 否 |
| 传输层一直没确认（`MICROUDS_TX_CONFIRM_TIMEOUT_MS`）或 `RespTransmit` 失败 | 是——钩子无法判断字节是否真的发出去了 |

钩子内部：

- 运行在 `TimerHandler` 上下文。可以不返回（复位/跳转）；如果返回，请求随后被释放。钩子执行期间新请求得到 `BUSY`。
- `10 xx` 之后，`ctx->session` 已经是**新**会话。
- `ctx` 里有意义的字段：`sid`、`sub_func`、`addr`、`req`/`req_len`、`session`、`sec_level`、`call_cnt`（服务回调被调用的次数）、`Userdata`、`resp_type`（正/负）、`nrc`（负响应时）。`payload`、`payload_cap`、`len` 不使用。

---

## 12. 请求处理流水线与 NRC 速查

每个请求都按下面的顺序检查，第一个失败就以对应的 NRC 结束请求。

| # | 检查 | NRC |
|---|---|---|
| 1 | 请求长度超过 `MICROUDS_REQUEST_BUF_SIZE` | `0x13` |
| 2 | 服务未注册 | `0x11` |
| 3 | 功能寻址请求，但服务没有 `FUNC_ADDR` | *忽略，不响应* |
| 4 | 服务在当前会话不可用 | `0x7F` |
| 5 | 有 `HAS_SUBFUNC`，但请求里没有子功能字节 | `0x13` |
| 6 | 注册了子功能表，但子功能不在表里 / 不在当前会话 | `0x12` / `0x7E` |
| 7 | 请求长度不在 `min_len … max_len` 内 | `0x13` |
| 8 | 服务或子功能的安全要求不满足 | `0x33` |
| 9 | 调用你的回调 | 回调返回什么就是什么 |

内核与模块产生的 NRC：

| NRC | 含义 | 来源 |
|---|---|---|
| `0x10` | 通用拒绝 | 回调为 `NULL` 的表项、种子生成未实现、`0x78` 次数过多、回调没有给出结果 |
| `0x11` | 服务不支持 | 流水线 |
| `0x12` | 子功能不支持 | 流水线、`0x10`、`0x27`、`0x3E`、`0x2C` |
| `0x13` | 长度/格式错误 | 流水线、`0x27`、DID 服务 |
| `0x14` | 响应过长 | 响应构造、`0x22`、`0x27` |
| `0x24` | 请求顺序错误 | `0x27` 没有对应种子就发送密钥 |
| `0x31` | 请求超出范围 | DID 服务（DID 不存在、当前会话不可用、动态定义非法……） |
| `0x33` | 安全访问拒绝 | 流水线、DID 服务 |
| `0x35` `0x36` `0x37` | 密钥无效 / 尝试次数过多 / 延时未到 | `0x27` |
| `0x78` | 响应挂起 | 内核（回调返回 *pending* 时） |
| `0x7E` `0x7F` | 当前会话下子功能 / 服务不支持 | 流水线 |

---

## 13. 内存与代码体积

默认配置、32 位指针下的实测值（`sizeof` 取自 `-m32` 编译结果）：

| 项目 | RAM |
|---|---:|
| 内核状态（`MicroUdsServer_Obj_t`） | 1744 B |
| DID 模块状态 | 1000 B |
| **合计** | **≈ 2.7 KB** |

如何随配置变化（32 位）：每个服务表项 92 B（× `MICROUDS_SERVICE_MAX_NUMBER`），每个子功能项 20 B（每服务 × `SUBFUNC_MAX_NUMBER`），请求缓冲 = `MICROUDS_REQUEST_BUF_SIZE`，响应缓冲 = `MICROUDS_RESPONSE_BUF_SIZE`（外加少量头部），每个 DID 项 24 B，每个动态 DID 76 B。

代码体积在 `-Os` 下约 9.4 KB（x86-32 实测，真实数字请用你自己的工具链测）。把 `MICROUDS_DID_ENABLE` 设为 `0` 可去掉 DID 模块。

没有动态内存分配、没有递归、没有浮点。

---

## 14. 线程/中断规则

- `MicroUdsServer_TimerHandler()` 必须始终在**同一个上下文**（主循环或同一个任务）里调用，不能在中断里调。
- `MicroUdsServer_RxIndication()` 和 `MicroUdsServer_HwRespTxDone()` 可以在中断或另一个任务里调用，但自身不能重入。它们只做"拷贝请求 / 置标志"。
- `MicroUdsServer_TickHandler()` 用于定时中断。
- 注册函数和 `Init` 必须在第一个请求到来之前执行；不要在 `TimerHandler` 运行期间调用。
- 你的回调（服务、DID、钩子、`RespTransmit`）都运行在 `TimerHandler` 里，不要阻塞。

---

## 15. 常见问题排查

| 现象 | 可能原因 |
|---|---|
| ECU 完全不回应 | `MicroUdsServer_RespTransmit` 没有被重写（函数名拼错，或重写放在了没被引用的静态库里）；没有调用 `TimerHandler`；`TickHandler` 没在跑。 |
| 每个请求都返回 `MICROUDS_BUSY` | 上一条响应一直没被确认：ISO-TP 发完后请调用 `MicroUdsServer_HwRespTxDone()`。不调用的话，内核要等 `MICROUDS_TX_CONFIRM_TIMEOUT_MS` 之后才恢复。 |
| `Register…` 函数返回 `MICROUDS_PARAM_ERR` | 具体清单见 9.1 和 10.1 节。常见原因：第二次 `Init` 之后才注册、SID/DID 重复。 |
| 我注册的东西全没了 | `MicroUdsServer_Init()` 会重置一切。每次都要在 `Init` 之后注册。 |
| `27 01` 回 `7F 27 10` | 没有实现 `MicroUdsServer_SecurityReqSeed`（默认实现报告长度 0）。 |
| `27 02` 总是回 `35` | 没有实现 `MicroUdsServer_SecurityGetKey`，或者你的算法和诊断仪的不一致。 |
| `22 xx xx` 在应该可用的会话里回 `0x31` | DID 的 `session` 掩码不包含当前会话。记住 `0` 表示*所有会话*。 |
| 请求回了 `0x7F` | 是*服务*的 `session_mask` 不包含当前会话（和 DID 的会话不同，DID 会话不对给的是 `0x31`）。 |
| 诊断仪被拒绝了，我的复位还是执行了 | 钩子对负响应也会调用；请判断 `ctx->resp_type == MICROUDS_RESPTYPE_POSITIVE`。 |
| 长的 DID 回 `0x14` | 增大 `MICROUDS_RESPONSE_BUF_SIZE`。 |
| 超过 64 字节的请求回 `0x13` | 增大 `MICROUDS_REQUEST_BUF_SIZE`。 |
| 弱函数出现重复定义的链接错误 | 你的工具链对弱符号的处理不同；请确认在你的目标平台上强定义能覆盖弱定义（库在 GCC 下测试过）。 |

---

## 16. Client（规划中）

诊断仪侧的 Client **尚未实现**。以下内容已预留：

- 源码目录 `Client/include` 和 `Client/src`，以及开关 `MICROUDS_CLIENT_ENABLE`（目前为 `0`）；
- `example/Client/`，用于放可运行的 Client 示例。

规划范围：构造请求、解析正/负响应（包括 `0x78`）、P2/P2\* 超时、TesterPresent 保活。Client 实现后，API 会写在本节。

---

## 17. 延伸阅读与许可证

- `doc/`：设计笔记（中文），包括 UDS 架构设计文档和 DID 知识文档。
- `example/Server/main.c`：本手册所有内容的参考实现。
- 许可证：见 [LICENSE](./LICENSE)