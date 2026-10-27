# MicroUds

> 一个轻量、可移植的 **UDS (ISO 14229-1) server** 面向微控制器的 UDS（ISO 14229-1）服务器，使用 C99 编写。

> 它只处理 *complete* 完整的 UDS PDU——分段和流量控制由你的 ISO-TP 协议栈负责（例如 MicroIsotp）。

[EN](./readme.md)

- **仅使用静态内存** — no `malloc`, about 2.7 KB RAM with the default configuration on a 32-bit MCU.

- **非阻塞、单线程内核** — one `TimerHandler()` call in your main loop 驱动全部功能.

- **与传输层无关** — 你只需提供一个“发送”函数，并在响应确实发送完成时通知它。

- **声明式服务** — sessions, security levels and addressing rules are *data* in a table, not code in your handlers.

- **内置** `0x10` `0x27` `0x3E` and a **DID 模块** (`0x22` `0x2E` `0x2C`) 支持针对每个 DID、每个方向分别进行访问控制。

---

## 目录

1. [状态](#1-状态)

2. [仓库目录结构](#2-仓库目录结构)

3. [整体工作方式](#3-整体工作方式)

4. [快速开始](#4-快速开始)

5. [移植指南](#5-移植指南)

6. [配置参考](#6-配置参考)

7. [核心概念](#7-核心概念)

8. [内置 services](#8-内置-services)

9. [编写自定义服务](#9-编写自定义服务)

10. [DID 模块（0x22 / 0x2E / 0x2C）](#10-did-模块0x22--0x2e--0x2c)

11. [响应发送后的处理](#11-响应发送后的处理)

12. [请求处理流程与 NRC 参考](#12-请求处理流程与-nrc-参考)

13. [内存与代码大小](#13-内存与代码大小)

14. [线程规则](#14-线程规则)

15. [故障排查](#15-故障排查)

16. [客户端（计划中）](#16-客户端计划中)

17. [延伸阅读与许可证](#17-延伸阅读与许可证)

---

## 1. 状态

| Area | State |

|---|---|

| `0x10` DiagnosticSessionControl, `0x27` SecurityAccess, `0x3E` TesterPresent | 已实现（内置） |

| `0x22` 按标识符读取数据, `0x2E` 按标识符写入数据, `0x2C` 动态定义数据标识符 | 已实现（DID 模块） |

| Your own services with sub-function tables | 已实现 |

| 会话超时（S3）、 `0x78` 响应等待、抑制肯定响应位、功能寻址规则 | 已实现 |

| 响应后钩子（复位/跳转至 Bootloader） | 已实现 |

| `0x31` RoutineControl, `0x11` ECUReset, `0x14` `0x19` DTC, `0x28`, `0x85`, `0x34`–`0x37` | **未内置** — 请自行注册（参见 [section 9](#9-编写自定义服务) and `example/Server`) |

| 客户端（诊断仪侧） | **计划中** — 参见 [section 16](#16-客户端计划中) |

| 多个服务器实例 | 不支持（仅支持单实例） |

| OEM-specific session IDs above 31 (e.g. `0x40`) | 不支持（会话采用位掩码，ID 范围为 1..31） |

| 可变长度 DID | 暂不支持（每个 DID 的长度固定） |

该库可以在以下编译选项下无警告编译： `-std=c99 -Wall -Wextra -Wpedantic -Wshadow -Wconversion`.

---

## 2. 仓库目录结构

```

MicroUds/

├── MicroUds_conf.h        # 配置（编辑此文件）

├── MicroUds_types.h       # 通用类型：状态码、NRC、会话、服务标志

├── MicroUds_utils.h       # 辅助宏（会话掩码、权限字节、tick 计算）

├── CMakeLists.txt

├── Server/

│   ├── include/

│   │   ├── MicroUdsServer.h            # public API  <- include this

│   │   ├── MicroUdsServer_types.h      # 服务表、回调上下文、内核状态

│   │   ├── MicroUdsServer_Did.h        # DID 模块 API

│   │   └── MicroUdsServer_Did_types.h  # DID tables and DID callback context

│   └── src/

│       ├── MicroUdsServer.c

│       └── MicroUdsServer_Did.c

├── Client/                # 预留（尚未实现）

├── example/

│   ├── Server/            # runnable PC example  <- start here

│   └── Client/            # reserved

└── doc/                   # 设计说明（中文）

```

---

## 3. 整体工作方式

```

 Your application         services, DID tables, security algorithm, hooks

        │  ▲

        ▼  │   MicroUdsServer_ServiceRegisterServ / _Did_RegisterReadItem / callbacks

 ┌────────────────────────────────────────────────────────────┐

 │ MicroUds Server                                            │

 │   dispatch pipeline · sessions · security · S3 · 0x78      │

 │   response queue · transmit-confirm · DID 模块           │

 └────────────────────────────────────────────────────────────┘

        │  ▲

        ▼  │   RespTransmit()  /  RxIndication()  /  HwRespTxDone()

 ISO-TP stack (MicroIsotp or any other)  ──  CAN driver

```

各函数由谁调用：

| 函数 | 调用方 | 调用位置 |

|---|---|---|

| `MicroUdsServer_Init()` | you | once at start-up, **before** registering anything |

| `MicroUdsServer_TickHandler()` | you | timer interrupt, at `MICROUDS_FREQ_HZ` |

| `MicroUdsServer_TimerHandler()` | 你 | 主循环/任务中（绝不能在 ISR 中调用） |

| `MicroUdsServer_RxIndication()` | ISO-TP | “完整消息已接收”回调（ISR 或任务） |

| `MicroUdsServer_HwRespTxDone()` | ISO-TP | “消息已发送”回调（ISR 或任务） |

| `MicroUdsServer_RespTransmit()` | the kernel | **you implement it** — hand the PDU to ISO-TP |

---

## 4. 快速开始

### 4.1 将库添加到构建中

**CMake**

```cmake

add_library(microuds STATIC

    MicroUds/Server/src/MicroUdsServer.c

    MicroUds/Server/src/MicroUdsServer_Did.c)

target_include_directories(microuds PUBLIC

    MicroUds                      # MicroUds_conf.h, MicroUds_types.h, MicroUds_utils.h

    MicroUds/Server/include)      # MicroUdsServer*.h

target_link_libraries(your_app PRIVATE microuds)

```

**Keil / IAR / Makefile** — add the two `.c` files and the two include directories above. The code needs C99, `stdint.h`, `stdbool.h`, and a compiler that supports `__attribute__((weak))`.

> **弱符号。** `RespTransmit`, `SecurityReqSeed`, `SecurityGetKey`, `OnResponseSent` and `Did_AddrReadable` 均为带有安全默认实现的弱函数。 Define your own version in your *application* sources. If you build MicroUds as a **static library**, do not put your overrides in another static library that nothing else references — the linker will not pull that object in and the weak default silently wins.

### 4.2 最小化集成

```c

#include "MicroUdsServer.h"

/* 1. Transmit: hand the response to your ISO-TP stack. Must not block. */

MicroUds_状态_t MicroUdsServer_RespTransmit(const uint8_t *pdu, uint32_t len)

{

    if (!isotp_is_idle())            return MICROUDS_BUSY;   /* 内核会在下一个周期重试 */

    isotp_send(pdu, len);            /* 你自己的 ISO-TP API */

    return MICROUDS_OK;

}

/* 2. ISO-TP callbacks */

void isotp_on_message_sent(void)                     { MicroUdsServer_HwRespTxDone(); }

void isotp_on_message_received(const uint8_t *d, size_t n, bool functional)

{

    MicroUdsServer_RxIndication(d, n, functional ? MICROUDS_ADDR_FUNC : MICROUDS_ADDR_PHY);

}

/* 3. Tick: call at MICROUDS_FREQ_HZ (default 1 kHz) */

void SysTick_Handler(void) { MicroUdsServer_TickHandler(); }

int main(void)

{

    MicroUdsServer_Init();                                   /* 始终最先调用 */

    MicroUdsServer_ServiceRegisterServ(my_services, MICROUDS_SIZEOF(my_services));

    MicroUdsServer_Did_RegisterReadItem(my_read_dids, MICROUDS_SIZEOF(my_read_dids));

    MicroUdsServer_Did_RegisterWriteItem(my_write_dids, MICROUDS_SIZEOF(my_write_dids));

    for (;;)

    {

        MicroUdsServer_TimerHandler();                       /* 驱动全部功能 */

        /* ... the rest of your main loop ... */

    }

}

```

That is enough to answer `10 01/02/03`, `3E 00`, `27 xx` (once you implement the two security functions, 参见 [8.2](#82-0x27-securityaccess)), and every DID you registered.

### 4.3 运行示例（无需硬件）

`example/Server/main.c` simulates an ISO-TP stack on a PC, 发送 scripted requests and prints every response.

```bash

cd example/Server

cmake -S . -B build

cmake --build build

./build/server_example

```

or without CMake:

```bash

gcc -std=c99 -I. -IServer/include \

    example/Server/main.c Server/src/MicroUdsServer.c Server/src/MicroUdsServer_Did.c \

    -o server_example

```

输出摘录：

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

The example is organised in six sections (port layer, security algorithm, DID tables, user services, response hook, demo driver). On real hardware you keep sections 2–5 and replace sections 1 and 6.

---

## 5. 移植指南

### 由你调用的函数

| Function | Notes |

|---|---|

| `MicroUds_状态_t MicroUdsServer_Init(void)` | Resets the kernel and registers the built-in services. Anything you registered earlier is discarded. |

| `void MicroUdsServer_TickHandler(void)` | Increments the internal tick. Call at exactly `MICROUDS_FREQ_HZ`. |

| `void MicroUdsServer_TimerHandler(void)` | The kernel main function. Each call advances the S3 timer, may dispatch one request and may advance one response step. Call it every millisecond or at least every few milliseconds (the advertised P2 is 50 ms). |

| `MicroUds_状态_t MicroUdsServer_RxIndication(const uint8_t *data, size_t len, MicroUds_AddrType_t addr)` | Feed one **complete** request PDU (SID first). The data is copied, so your buffer can be reused immediately. Returns `MICROUDS_OK`; `MICROUDS_BUSY` if the previous request is still in progress (including *waiting for its response to be sent*) — the request is dropped, you decide whether to retry or answer `0x21`; `MICROUDS_PARAM_ERR` for a null pointer or zero length. |

| `void MicroUdsServer_HwRespTxDone(void)` | Tell the kernel the response given to `RespTransmit` has been sent (or failed). Only sets a flag, so it is safe in an ISR. |

| `uint8_t MicroUdsServer_GetSession(void)` | Current session number. |

| `uint8_t MicroUdsServer_GetSecurityLevel(void)` | Currently unlocked level, `0` = locked. |

`addr` is `MICROUDS_ADDR_PHY` (physical) or `MICROUDS_ADDR_FUNC` (functional). The kernel needs it because the two have different response rules (参见 [7.4](#74-物理寻址与功能寻址)).

### 由你实现的函数（弱函数）

| Function | Required? | Purpose |

|---|---|---|

| `MicroUds_状态_t MicroUdsServer_RespTransmit(const uint8_t *pdu, uint32_t len)` | **Yes** | Give the response to ISO-TP. Return `MICROUDS_OK` when accepted (then call `HwRespTxDone` when it is on the wire), `MICROUDS_BUSY` if the transport cannot take it now (内核会在下一个周期重试), anything else to drop the response. The default does nothing and returns OK, so **forgetting this produces silence, not an error**. |

| `void MicroUdsServer_SecurityReqSeed(uint8_t level, uint8_t *out, uint32_t *len)` | If you use `0x27` | Generate a 参见d. `*len` is the buffer capacity (`MICROUDS_SEED_SIZE`) on entry; set it to the number of bytes written. 设置ting `0` means "failed". The default sets `0`, so `27 01` answers `0x10` until you implement it. |

| `void MicroUdsServer_SecurityGetKey(uint8_t level, const uint8_t *参见d, uint32_t 参见d_len, uint8_t *key, uint32_t *key_len)` | If you use `0x27` | Compute the key the tester must send for that 参见d. `*key_len` is the capacity (`MICROUDS_KEY_SIZE`) on entry; set it to the key length. The default sets `0`, so every key is rejected (safe default). |

| `void MicroUdsServer_OnResponseSent(MicroUdsServer_ServiceCtx_t *ctx)` | Optional | See [section 11](#11-响应发送后的处理). |

| `bool MicroUdsServer_Did_AddrReadable(uint32_t addr, uint32_t len)` | Only if `MICROUDS_DID_DYNA_BY_ADDR = 1` | Whitelist for `0x2C 02`. The default denies everything. |

Seeds must come from a hardware RNG and the key algorithm should be a real MAC (e.g. AES-CMAC). The XOR in the example is for demonstration only.

---

## 6. 配置参考

Edit `MicroUds_conf.h`. Time values are in milliseconds and converted with `MICROUDS_MS_TO_TICK()`.

| Macro | Default | Meaning |

|---|---|---|

| `MICROUDS_SERVER_ENABLE` | `1` | Build the server. |

| `MICROUDS_CLIENT_ENABLE` | `0` | Reserved for the client. |

| `MICROUDS_FREQ_HZ` | `1000` | Tick frequency; must match how often you call `TickHandler`. |

| `MICROUDS_SESSION_HOLD_MS` | `5000` | S3: idle time in a non-default session before falling back to the default session. |

| `MICROUDS_P2_SERVER_MS` | `50` | P2 reported in the `0x10` response. |

| `MICROUDS_P2STAR_SERVER_MS` | `5000` | P2\* reported in the `0x10` response. |

| `MICROUDS_PENDING_RESEND_MS` | `4000` | While a service is pending, repeat `0x78` this often. Must be smaller than `P2STAR`. |

| `MICROUDS_PENDING_MAX_COUNT` | `10` | 在……之后 this many `0x78` the kernel gives up and answers `0x10`. |

| `MICROUDS_TX_CONFIRM_TIMEOUT_MS` | `1000` | Maximum wait for `HwRespTxDone` after `RespTransmit` returned OK; afterwards the response is considered finished. |

| `MICROUDS_REQUEST_BUF_SIZE` | `64` | Largest request (full PDU, bytes). Longer requests are answered `0x13`. |

| `MICROUDS_RESPONSE_BUF_SIZE` | `64` | Largest response (full PDU, bytes). |

| `MICROUDS_SESSION_MASK` | `4` | **Highest** session number supported: `4` means sessions 1, 2, 3, 4. |

| `MICROUDS_SEED_SIZE` / `MICROUDS_KEY_SIZE` | `4` / `4` | Security 参见d and key length. |

| `MICROUDS_SECURITY_MAX_LEVEL` | `4` | Number of security levels (`27 01/02` = level 1 … ). |

| `MICROUDS_SECURITY_MAX_ATTEMPTS` | `3` | Failed keys before lock-out. |

| `MICROUDS_SECURITY_DELAY_MS` | `10000` | Lock-out time after too many failures. |

| `MICROUDS_SERVICE_MAX_NUMBER` | `16` | Size of the service table. **内置 services use 3 (6 with the DID 模块)**, leaving 10 for yours. |

| `MICROUDS_SUBFUNC_MAX_NUMBER` | `3` | Sub-functions per service table entry. |

| `MICROUDS_DID_ENABLE` | `1` | Build the DID 模块. |

| `MICROUDS_DID_READ_SIZE` / `MICROUDS_DID_WRITE_SIZE` | `8` / `8` | Capacity of the read / write DID tables. |

| `MICROUDS_DID_DYNAMICALLY_SIZE` | `8` | Number of dynamic DIDs. |

| `MICROUDS_DID_DYNAMICALLY_ITEM_LEN` | `5` | Segments per dynamic DID. |

| `MICROUDS_DID_READ_MAX_PER_REQ` | `8` | Most DIDs in one `0x22`; more is answered `0x31`. |

| `MICROUDS_DID_READ_PARTIAL` | `0` | `0` = if any DID of a `0x22` is unknown/unavailable, the whole request fails. `1` = skip those, answer the rest. |

| `MICROUDS_DID_NRC_SESSION` | `0x31` | NRC when a DID exists but is not available in the current session. |

| `MICROUDS_DID_DYNA_ID_MIN` / `_MAX` | `0xF300` / `0xF3FF` | DID range allowed for dynamic DIDs. |

| `MICROUDS_DID_DYNA_BY_ADDR` | `1` | Allow `0x2C 02` (dynamic DID by memory address). **Exposes memory to the tester — 严格限制白名单范围, or set `0`.** |

| `MICROUDS_DID_DYNA_CLEAR_ON_DEFAULT` | `1` | Delete all dynamic DIDs when the server returns to the default session. |

| `MICROUDS_DID_READ_REQ_MIN_LEN` / `WRITE_REQ_MIN_LEN` / `DYNA_REQ_MIN_LEN` | `3` / `4` / `2` | Minimum request lengths used by the built-in DID services. Normally not changed. |

| `MICROUDS_DID_RESP_MAX_LEN` / `RESP_MIN_LEN` | `32` / `2` | Unused (kept for compatibility). |

---

## 7. 核心概念

### 7.1 一次处理一个请求

The kernel owns one request buffer and one response buffer. A request is "in progress" from `RxIndication` until its **final response has been sent** (`HwRespTxDone`), so a new request arriving earlier gets `MICROUDS_BUSY`. A `0x78` ResponsePending is not a final response: the request stays alive after it is sent.

### 7.2 会话

Session numbers are the values used in `10 xx`:

| Constant | Value | 典型用途 |

|---|---|---|

| `MICROUDS_SESSION_DEFAULT` | 1 | 上电默认会话 |

| `MICROUDS_SESSION_PROGRAM` | 2 | 编程 |

| `MICROUDS_SESSION_EXTEND` | 3 | 扩展诊断 |

| `MICROUDS_SESSION_SECURITY` | 4 | 安全系统诊断 |

Configuration fields that say "session mask" use one bit per session:

```c

uint32_t mask = MICROUDS_SET_SESSION(MICROUDS_SESSION_DEFAULT) |

                MICROUDS_SET_SESSION(MICROUDS_SESSION_EXTEND);

```

内核实现的规则：

- The server powers up in the default session. A `10 xx` is answered first; the session changes **after the response has been sent**.

- Whenever the session actually changes, **security is locked again** (the failed-attempt counter is not reset), and — with `MICROUDS_DID_DYNA_CLEAR_ON_DEFAULT` — dynamic DIDs are deleted when going back to the default session.

- Requesting the session you are already in does not lock security (OEM specifications differ here; change `MicroUdsServer_SessionSwitch` if yours says otherwise).

- **S3**: in a non-default session, if nothing happens for `MICROUDS_SESSION_HOLD_MS`, the server returns to the default session. The timer restarts on every received request and when a request finishes, and does not run while a request is in progress.

- P2/P2\* are only *reported* in the `0x10` response. The kernel does not time P2; for anything that may take longer than P2, return **pending** (section 9.3).

### 7.3 安全等级和权限字节

`27 01/02` unlocks level 1, `27 03/04` level 2, and so on. **Only one level is active at a time**: unlocking another level replaces the current one.

A *permission byte* (`permis`, used in service tables, sub-function tables and DID tables) says which unlocked levels may use the item:

| Value | Helper | Meaning |

|---|---|---|

| `0` | — | No unlock needed |

| `n` | `MICROUDS_PERMIS_ATLEAST(n)` | Current level must be **≥ n** |

| `0x80 \| n` | `MICROUDS_PERMIS_EXACT(n)` | Current level must be **exactly n** |

### 7.4 物理寻址与功能寻址

- A service accepts functional requests only if it has `MICROUDS_SERV_FLAG_FUNC_ADDR`; otherwise the request is silently ignored.

- 内置: `0x10` and `0x3E` accept functional addressing. `0x27`, `0x22`, `0x2E`, `0x2C` do not.

- For functional requests, negative responses with NRC `0x11 0x12 0x31 0x7E 0x7F` are **not sent** (so a broadcast does not make every ECU answer "not supported"). Positive responses are sent normally.

### 7.5 抑制肯定响应

If a service has `MICROUDS_SERV_FLAG_SUPPRESS` and the request's sub-function has bit 7 set (`3E 80`, `10 83`, `11 81` …), the positive response is not sent. 否定响应s are always sent. The service still runs, and `OnResponseSent` is still called. Sub-function numbers of such services must be below `0x80`.

---

## 8. 内置 services

| Service | 会话 | Functional | Suppress | 请求长度 | 行为 |

|---|---|---|---|---|---|

| `0x10` | 所有受支持的会话 | yes | yes | exactly 2 | 肯定响应： `50 <session> <P2 hi lo> <P2* hi lo>` (P2\* in 10 ms units). 不支持的会话 → `0x12`. |

| `0x27` | 编程, Extended | no | no | ≥ 2 | See below. |

| `0x3E` | 所有受支持的会话 | yes | yes | exactly 2 | 仅支持子功能 `00`; others → `0x12`. |

| `0x22` `0x2E` `0x2C` | 参见 [section 10](#10-did-模块0x22--0x2e--0x2c) | | | | DID 模块 |

### 8.1 `0x10` DiagnosticSessionControl

无需配置。 支持的会话范围为 `1 … MICROUDS_SESSION_MASK`.

### 8.2 `0x27` SecurityAccess

- 奇数子功能 = requestSeed（请求种子），其后的偶数子功能 = sendKey（发送密钥） (`27 01` / `27 02` for level 1).

- 有效子功能： `1 … 2 × MICROUDS_SECURITY_MAX_LEVEL`; otherwise `0x12`.

- requestSeed：如果延迟定时器正在运行 → `0x37`; if that level is already unlocked → an all-zero 参见d; otherwise your `SecurityReqSeed` is called (if it reports length 0 → `0x10`).

- sendKey 必须紧接在 requestSeed 之后 **for the same level**, otherwise `0x24`. Key length must be `MICROUDS_KEY_SIZE`, otherwise `0x13` (the 参见d stays valid). Your `SecurityGetKey` computes the expected key; wrong key → `0x35`.

- A 参见d is single use: after any sendKey it is discarded, success or not.

- 在……之后 `MICROUDS_SECURITY_MAX_ATTEMPTS` 次连续失败后，响应为 `0x36` and requestSeed answers `0x37` for `MICROUDS_SECURITY_DELAY_MS`. Session changes and S3 timeouts do **not** reset the counter.

---

## 9. 编写自定义服务

### 9.1 服务表

```c

static const MicroUdsServer_ServiceTable_t my_services[] = {

    {

        .sid          = 0x11,                                          /* 服务 ID */

        .flags        = MICROUDS_SERV_FLAG_HAS_SUBFUNC |               /* 第 2 个字节是子功能  */

                        MICROUDS_SERV_FLAG_SUPPRESS,                   /* 子功能的 bit 7 表示抑制响应 */

        .permis       = 0,                                             /* 安全访问要求（7.3） */

        .session_mask = MICROUDS_SET_SESSION(MICROUDS_SESSION_EXTEND) |

                        MICROUDS_SET_SESSION(MICROUDS_SESSION_PROGRAM),

        .min_len      = 2,                                             /* 完整 PDU 长度，包含 SID；0 表示不检查 */

        .max_len      = 2,

        .callback     = svc_ecu_reset,

        .Userdata     = NULL,

    },

};

MicroUdsServer_ServiceRegisterServ(my_services, MICROUDS_SIZEOF(my_services));   /* after Init */

```

| 字段 | 含义 |

|---|---|

| `sid` | Service ID. 不得与内置服务或其他表项冲突。 |

| `flags` | `HAS_SUBFUNC`: the second request byte is a sub-function — the kernel parses it, handles the suppress bit and **echoes it in the positive response**. `SUPPRESS`: the sub-function's bit 7 is the suppress bit (requires `HAS_SUBFUNC`). `FUNC_ADDR`: accept functional addressing. Do not set `BUILTIN`. |

| `permis` | 整个服务的安全访问要求。 |

| `session_mask` | 会话 in which the service exists. Anything else → `0x7F`. A value of `0` means "never available" (`0x11`). |

| `min_len` / `max_len` | 完整 PDU 长度，包含 SID. Violations → `0x13`. |

| `callback` | 请求通过所有检查后调用。 May be `NULL` only if you register a sub-function table. |

| `Userdata` | 作为 `ctx->Userdata`. |

`MicroUdsServer_ServiceRegisterServ` is **atomic** (nothing is registered if any entry is invalid) and can be called several times. It returns `MICROUDS_PARAM_ERR` for a null/empty table, a full service table, a duplicate SID, the `BUILTIN` flag, or `SUPPRESS` without `HAS_SUBFUNC`.

### 9.2 回调上下文

```c

static void svc_ecu_reset(MicroUdsServer_ServiceCtx_t *ctx)

{

    if (ctx->sub_func == 0x01)

    {

        MicroUdsServer_CtxPositive(ctx, 0);                 /* answers 51 01 */

        return;

    }

    MicroUdsServer_CtxNegative(ctx, MICROUDS_NRC_SFNS);     /* answers 7F 11 12 */

}

```

**You must always set a result.** The context starts as "negative, `0x10`"; call exactly one of:

| Helper | Result |

|---|---|

| `MicroUdsServer_CtxPositive(ctx, len)` | 肯定响应。 The `len` bytes you wrote to `ctx->payload` are appended after the SID (and sub-function). |

| `MicroUdsServer_CtxNegative(ctx, nrc)` | 否定响应 `7F <sid> <nrc>`. |

| `MicroUdsServer_CtxPending(ctx)` | 尚未完成 — 参见 9.3. |

输入（只读）：

| 字段 | 含义 |

|---|---|

| `sid`, `sub_func` | Service ID and sub-function (suppress bit already removed; `0` if the service has none). |

| `req`, `req_len` | Request bytes **after** the SID (and after the sub-function if `HAS_SUBFUNC`). |

| `addr` | `MICROUDS_ADDR_PHY` or `_FUNC`. |

| `session`, `sec_level` | Current session number and unlocked level. |

| `call_cnt` | `0` on the first call, then 1, 2, … on each re-entry after *pending*. |

| `Userdata` | From the table entry. |

输出： `payload` points into the response buffer (already past the SID/sub-function), `payload_cap` is the space left; write there, then call `CtxPositive` with the length. If the answer would not fit, the kernel replies `0x14`.

> 你的回调在以下函数的上下文中运行： `TimerHandler`. Keep them short and non-blocking.

### 9.3 耗时操作： *pending*

```c

static void rc_erase(MicroUdsServer_ServiceCtx_t *ctx)

{

    if (!flash_erase_done())

    {

        MicroUdsServer_CtxPending(ctx);          /* kernel 发送 7F <sid> 78 immediately */

        return;                                  /* ...and calls this function again every cycle */

    }

    MicroUdsServer_CtxPositive(ctx, 0);          /* finally: the real response */

}

```

- 第一次 `CtxPending` 发送 `0x78` 立即发送。 随后每次调用时，都会再次调用回调 `TimerHandler` pass (`ctx->call_cnt` increases) 直到它返回肯定响应或否定响应。

- Start the real work when `call_cnt == 0`; keep state in your own variables (the context is rebuilt each call).

- `0x78` is repeated every `MICROUDS_PENDING_RESEND_MS`; after `MICROUDS_PENDING_MAX_COUNT` repeats the kernel ends the request with `0x10`.

- New requests get `BUSY` while pending.

### 9.4 子功能表

If every sub-function has its own handler or its own permissions, register a table (service entry with `.callback = NULL` or a default handler):

```c

static const MicroUdsServer_SubFuncTable_t rc_subs[] = {

    { .id = 0x01, .callback = rc_start,   .permis = MICROUDS_PERMIS_ATLEAST(1) },  /* 需要安全等级 1 */

    { .id = 0x02, .callback = rc_stop    },

    { .id = 0x03, .callback = rc_results },

};

MicroUdsServer_ServiceRegisterSubFunc(0x31, rc_subs, MICROUDS_SIZEOF(rc_subs));

```

| 字段 | 含义 |

|---|---|

| `id` | Sub-function number (below `0x80` if the service has `SUPPRESS`). |

| `callback` | Handler for this sub-function. **Must not be `NULL`** (a `NULL` callback answers `0x10`; there is no fallback to the service callback). |

| `Userdata` | Passed as `ctx->Userdata`. |

| `session_mask` | `0` = same as the service; otherwise the sessions where this sub-function exists. Present but not in the current session → `0x7E`. |

| `permis` | **Additional** security requirement for this sub-function. |

Once a table is registered, a sub-function that is not in it is answered `0x12`. The table is replaced if you register again. Only user services with `HAS_SUBFUNC` can have a table (at most `MICROUDS_SUBFUNC_MAX_NUMBER` entries).

Without a table, one service callback receives every sub-function in `ctx->sub_func` (this is how the built-in services work).

`example/Server/main.c` contains a complete `0x31` RoutineControl built this way, including a slow routine using *pending*.

---

## 10. DID 模块（0x22 / 0x2E / 0x2C）

### 10.1 定义 DID

Read and write tables are separate. **未出现在写表中的 DID 为只读。** 同一个 DID 可以同时出现在两个表中，并设置不同的权限。

```c

static uint8_t g_vin[17] = { 'M','I','C','R','O','U','D','S','0','0','0','0','0','0','0','0','1' };

static uint8_t g_cfg[2];

static const MicroUdsServerDidItem_Config_t read_dids[] = {

    { .Identifier = 0xF190, .data = g_vin, .data_len = sizeof g_vin },                 /* 所有会话均可用，无需解锁 */

    { .Identifier = 0x0100, .data = g_cfg, .data_len = sizeof g_cfg,

      .session = MICROUDS_SET_SESSION(MICROUDS_SESSION_EXTEND) },                      /* 仅扩展会话 */

};

static const MicroUdsServerDidItem_Config_t write_dids[] = {

    { .Identifier = 0x0100, .data = g_cfg, .data_len = sizeof g_cfg,

      .session = MICROUDS_SET_SESSION(MICROUDS_SESSION_EXTEND),

      .permis  = MICROUDS_PERMIS_ATLEAST(1) },                                         /* 扩展会话且已解锁 */

};

MicroUdsServer_Did_RegisterReadItem (read_dids,  MICROUDS_SIZEOF(read_dids));   /* after Init */

MicroUdsServer_Did_RegisterWriteItem(write_dids, MICROUDS_SIZEOF(write_dids));

```

| 字段 | 含义 |

|---|---|

| `Identifier` | 16-bit DID. |

| `session` | DID 可用的会话掩码。 **`0` = all sessions.** |

| `permis` | Security requirement, 参见 7.3. **`0` = no unlock.** |

| `data` / `data_len` | Your variable and its size. DIDs have a **fixed length**. |

| `cb` | Optional callback, 参见 10.2. |

| `Userdata` | Passed to the callback as `ctx->Userdata`. |

> `session = 0` and `permis = 0` mean *no restriction*. A forgotten field opens the DID, so 对任何敏感 DID 都应显式设置这两个字段.

Registration can be called several times (tables are appended) and is atomic. It returns `MICROUDS_PARAM_ERR` for: empty table, no room left, duplicate DID, `data_len == 0`, a **read** item with `data == NULL`, a read item longer than `MICROUDS_RESPONSE_BUF_SIZE − 3`, a **write** item with neither `data` nor `cb`, or a write item that does not fit in the request buffer.

The access check is: *service* conditions (the built-in `0x22`/`0x2E` are open in all sessions with no unlock) **and** the DID's own `session`/`permis`. The real policy lives on each DID.

### 10.2 DID 回调

The callback is invoked **before** the kernel copies any data.

```c

typedef struct {

    /* inputs */

    uint16_t Did;  uint16_t Did_len;          /* the DID; number of DIDs in this request */

    uint8_t *data; uint16_t data_len;         /* your variable (may be NULL for write-only items) */

    void *Userdata;

    bool is_write;

    const uint8_t *in_data; uint16_t in_len;  /* write: the bytes sent by the tester (read-only) */

    uint8_t session, sec_level;

    uint32_t call_cnt;                        /* write + pending: 0 first, then 1, 2 ... */

    /* outputs */

    uint8_t nrc;                              /* set to reject */

    bool pending;                             /* write only: not finished yet */

} MicroUdsServerDid_Ctx_t;

```

- **Read**: 刷新 `ctx->data` 为当前值 (e.g. a sensor). 随后内核会复制 `data` into the response. 设置 `ctx->nrc` 以拒绝请求。 读取操作不支持 pending (answers `0x10`).

- **Write**: 校验 `ctx->in_data` (range, state …). 如果保持 `nrc == 0` the write is **accepted** and the kernel copies `in_data` into `data` (if non-`NULL`). If you set `nrc`, 不会修改任何内容。 If your variable is not plain RAM (e.g. NVM), pass `data = NULL` and store the bytes yourself in the callback.

- **Slow writes**: set `ctx->pending = true`. The kernel answers `0x78`, calls the callback again every cycle (`call_cnt` increments) and accepts the write when it returns without `pending`.

### 10.3 `0x22` 按标识符读取数据

`22 <DID> [<DID> ...]` → `62 <DID> <data> [<DID> <data> ...]`

- 请求长度 must be `1 + 2N` (else `0x13`), with `N ≤ MICROUDS_DID_READ_MAX_PER_REQ` (else `0x31`).

- **Two phases**: first every DID is checked (exists, session, security, total length) *without* reading anything, then the data is read. So a bad DID never leaves you with half-executed read callbacks.

- Unknown DID, or DID not available in this session → `0x31` (the session NRC is `MICROUDS_DID_NRC_SESSION`). With `MICROUDS_DID_READ_PARTIAL = 1` such DIDs are skipped and the rest answered (`0x31` only if nothing is left).

- Security not satisfied for **any** DID → `0x33` for the whole request, never partial data.

- Response too large for `MICROUDS_RESPONSE_BUF_SIZE` → `0x14`.

- Static and dynamic DIDs can be mixed in one request.

### 10.4 `0x2E` 按标识符写入数据

`2E <DID> <data>` → `6E <DID>`

- The DID must be in the write table, otherwise `0x31` (this also protects dynamic DIDs, which can never be written).

- Order of checks: DID exists → session/security (`0x31` / `0x33`) → length must equal `data_len` exactly (`0x13`) → your callback.

### 10.5 `0x2C` 动态定义数据标识符

允许诊断仪使用现有 DID 的部分数据组合出一个新的 DID。 Available in the 编程 and Extended sessions.

| 子功能 | 请求 | 说明 |

|---|---|---|

| `01` define by identifier | `2C 01 <DDDID> { <srcDID> <pos> <size> } × N` | `pos` starts at **1**. Up to `MICROUDS_DID_DYNAMICALLY_ITEM_LEN` segments. |

| `02` define by address | `2C 02 <DDDID> <ALFID> { <addr> <size> } × N` | Only with `MICROUDS_DID_DYNA_BY_ADDR = 1`. ALFID must be `0x24` or `0x14`-style: **4-byte address**, 1- or 2-byte size. Otherwise `0x12`/`0x31`. |

| `03` clear | `2C 03` or `2C 03 <DDDID>` | Without a DID: clear all. Unknown DID → `0x31`. |

肯定响应： `6C <sub> <DDDID>` (`6C 03` for clear-all).

规则：

- `DDDID` must be inside `MICROUDS_DID_DYNA_ID_MIN … MAX` and must not be a static DID.

- Every source DID must be readable *now* (session + security). The check is **repeated every time the dynamic DID is read**, so a dynamic DID can never be used to get around a source DID's protection.

- 总长度必须能够容纳在响应缓冲区中。 Defining an existing `DDDID` again **replaces** it. 定义失败时不会产生任何更改。

- All dynamic DIDs are deleted when returning to the default session (`MICROUDS_DID_DYNA_CLEAR_ON_DEFAULT`).

- Sub-function `02` reads **raw memory**. The only protection is your `MicroUdsServer_Did_AddrReadable()` whitelist, which denies everything by default, and the 编程/Extended-session requirement; the 当前实现不会要求进行安全解锁. 设置 `MICROUDS_DID_DYNA_BY_ADDR` to `0` if you do not need it.

```c

bool MicroUdsServer_Did_AddrReadable(uint32_t addr, uint32_t len)

{

    return (addr >= 0x20000000u) && (len <= 0x100u) && ((addr - 0x20000000u) <= (0x100u - len));

}

```

---

## 11. 响应发送后的处理

Resetting the ECU or jumping to the bootloader must happen **after** the positive response has left the bus. Implement one hook for all services (built-in and yours):

```c

void MicroUdsServer_OnResponseSent(MicroUdsServer_ServiceCtx_t *ctx)

{

    if (ctx->resp_type != MICROUDS_RESPTYPE_POSITIVE)      /* IMPORTANT: negative responses also arrive here */

    {

        return;

    }

    if (ctx->sid == 0x11 && ctx->sub_func == 0x01) { NVIC_SystemReset(); }

    if (ctx->sid == 0x10 && ctx->sub_func == 0x02) { jump_to_bootloader(); }

}

```

调用时机：

| 情况 | 是否调用钩子？ |

|---|---|

| A service callback ran and its final response (positive **or negative**) was sent | yes |

| The positive response was suppressed (`11 81`) | yes — 仍然必须执行复位 |

| `0x78` ResponsePending | no |

| The request was rejected by the pipeline before reaching a service (unknown SID, wrong session, bad length, security denied …) | no |

| A functional request that was silently ignored | no |

| The transport never confirmed (`MICROUDS_TX_CONFIRM_TIMEOUT_MS`) or `RespTransmit` failed | yes — the hook cannot tell whether the bytes really left |

在钩子函数内部：

- It runs in `TimerHandler` context. It may never return (reset / jump). If it returns, the request is released afterwards. New requests get `BUSY` while it runs.

- `ctx->session` is already the **new** session after a `10 xx`.

- Valid `ctx` fields: `sid`, `sub_func`, `addr`, `req`/`req_len`, `session`, `sec_level`, `call_cnt` (how many times the service callback ran), `Userdata`, `resp_type` (positive or negative), `nrc` (when negative). `payload`, `payload_cap`, `len` are not used.

---

## 12. 请求处理流程与 NRC 参考

Every request goes through these checks, in this order. 第一次 failure ends the request with the listed NRC.

| # | Check | NRC |

|---|---|---|

| 1 | Request longer than `MICROUDS_REQUEST_BUF_SIZE` | `0x13` |

| 2 | Service not registered | `0x11` |

| 3 | Functional request and service lacks `FUNC_ADDR` | *ignored, no response* |

| 4 | Service not available in the current session | `0x7F` |

| 5 | `HAS_SUBFUNC` but the request has no sub-function byte | `0x13` |

| 6 | Sub-function table registered and sub-function missing / not in this session | `0x12` / `0x7E` |

| 7 | 请求长度 outside `min_len … max_len` | `0x13` |

| 8 | Service or sub-function security not satisfied | `0x33` |

| 9 | Your callback runs | whatever it returns |

NRCs produced by the kernel and modules:

| NRC | Meaning | Produced by |

|---|---|---|

| `0x10` | General reject | callback without `NULL` check, 参见d/key not implemented, too many `0x78`, a result never set |

| `0x11` | Service not supported | pipeline |

| `0x12` | Sub-function not supported | pipeline, `0x10`, `0x27`, `0x3E`, `0x2C` |

| `0x13` | Wrong length / format | pipeline, `0x27`, DID services |

| `0x14` | Response too long | response builder, `0x22`, `0x27` |

| `0x24` | Request sequence error | `0x27` sendKey without a matching 参见d |

| `0x31` | Request out of range | DID services (unknown DID, DID not in this session, bad dynamic definition …) |

| `0x33` | Security access denied | pipeline, DID services |

| `0x35` `0x36` `0x37` | Invalid key / too many attempts / delay running | `0x27` |

| `0x78` | Response pending | kernel, when a callback returns *pending* |

| `0x7E` `0x7F` | Sub-function / service not supported in the active session | pipeline |

---

## 13. 内存与代码大小

在默认配置、32 位指针条件下测量 (`sizeof` read from a `-m32` build):

| Item | RAM |

|---|---:|

| Kernel state (`MicroUdsServer_Obj_t`) | 1744 B |

| DID 模块 state | 1000 B |

| **Total** | **≈ 2.7 KB** |

内存占用的扩展规律（32 位）： each service table entry 92 B (× `MICROUDS_SERVICE_MAX_NUMBER`), each sub-function entry 20 B (× `SUBFUNC_MAX_NUMBER` per service), request buffer = `MICROUDS_REQUEST_BUF_SIZE`, response buffer = `MICROUDS_RESPONSE_BUF_SIZE` (plus small headers), each DID item 24 B, each dynamic DID 76 B.

代码大小约为 9.4 KB at `-Os` (measured for x86-32; use your own toolchain for a real number). 设置ting `MICROUDS_DID_ENABLE` to `0` removes the DID 模块.

不使用动态内存分配、不使用递归、不使用浮点运算。

---

## 14. 线程规则

- `MicroUdsServer_TimerHandler()` must always be called from the **same context** (main loop or one task), never from an ISR.

- `MicroUdsServer_RxIndication()` and `MicroUdsServer_HwRespTxDone()` 可以从 ISR 或其他任务中调用, but not re-entrantly with themselves. They only copy the request / set a flag.

- `MicroUdsServer_TickHandler()` is meant for a timer ISR.

- Registration functions and `Init` 必须在收到第一个请求之前执行; do not call them while `TimerHandler` is running.

- Your callbacks (service, DID, hook, `RespTransmit`) run inside `TimerHandler`. Do not block in them.

---

## 15. 故障排查

| Symptom | 可能原因 |

|---|---|

| ECU 没有任何响应 | `MicroUdsServer_RespTransmit` is not being overridden (misspelled name, or the override is in an unreferenced static library); `TimerHandler` is not being called; `TickHandler` is not running. |

| 每个请求都返回 `MICROUDS_BUSY` | The previous response was never confirmed: call `MicroUdsServer_HwRespTxDone()` when ISO-TP finishes sending. Without it the kernel only recovers after `MICROUDS_TX_CONFIRM_TIMEOUT_MS`. |

| A `Register…` function returns `MICROUDS_PARAM_ERR` | See sections 9.1 and 10.1 for the exact list. A common one: registering after a second `Init`, or a duplicate SID/DID. |

| 我注册的内容全部消失了 | `MicroUdsServer_Init()` resets everything. Register after `Init`, every time. |

| `27 01` answers `7F 27 10` | `MicroUdsServer_SecurityReqSeed` is not implemented (the default reports length 0). |

| `27 02` 始终响应 `35` | `MicroUdsServer_SecurityGetKey` is not implemented, or your algorithm differs from the tester's. |

| `22 xx xx` answers `0x31` in a session where it should work | The DID's `session` mask does not include the current session. Remember `0` means *all*. |

| A request answered `0x7F` | The *service's* `session_mask` does not include the current session (this is different from a DID's session, which gives `0x31`). |

| My reset runs even when the tester was rejected | The hook is also called for negative responses; test `ctx->resp_type == MICROUDS_RESPTYPE_POSITIVE`. |

| 较长的 DID 失败 with `0x14` | Increase `MICROUDS_RESPONSE_BUF_SIZE`. |

| 长度超过 64 bytes fail with `0x13` | Increase `MICROUDS_REQUEST_BUF_SIZE`. |

| 重复符号链接错误 for the weak functions | Your toolchain handles weak symbols differently; confirm that a strong definition overrides a weak one on your target (the library is tested with GCC). |

---

## 16. 客户端（计划中）

The tester-side client is **not implemented yet**. The following is reserved:

- source directories `Client/include` and `Client/src`, and the switch `MICROUDS_CLIENT_ENABLE` (currently `0`);

- `example/Client/` for a runnable client example.

计划实现的范围： build requests, parse positive/negative responses (including `0x78`), P2/P2\* timeouts and TesterPresent keep-alive. The API will be documented in this section when it exists.

---

## 17. 延伸阅读与许可证

- `doc/` — design notes (in Chinese): the UDS architecture notes and the DID knowledge document.

- `example/Server/main.c` — 本手册所有内容的参考实现。

- License: 参见 [LICENSE](LICENSE).
