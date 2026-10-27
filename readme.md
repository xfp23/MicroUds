# MicroUds

> A small, portable **UDS (ISO 14229-1) server** for microcontrollers, written in C99.
> It handles *complete* UDS PDUs only — segmentation and flow control belong to your ISO-TP stack (e.g. MicroIsotp).

[中文文档](readme.zh.md)

- **Static memory only** — no `malloc`, about 2.7 KB RAM with the default configuration on a 32-bit MCU.
- **Non-blocking, single-threaded kernel** — one `TimerHandler()` call in your main loop drives everything.
- **Transport independent** — you give it a "transmit" function and tell it when a response has really been sent.
- **Declarative services** — sessions, security levels and addressing rules are *data* in a table, not code in your handlers.
- **Built-in** `0x10` `0x27` `0x3E` and a **DID module** (`0x22` `0x2E` `0x2C`) with per-DID, per-direction access control.

---

## Contents

1. [Status](#1-status)
2. [Repository layout](#2-repository-layout)
3. [How it fits together](#3-how-it-fits-together)
4. [Quick start](#4-quick-start)
5. [Porting guide](#5-porting-guide)
6. [Configuration reference](#6-configuration-reference)
7. [Core concepts](#7-core-concepts)
8. [Built-in services](#8-built-in-services)
9. [Writing your own services](#9-writing-your-own-services)
10. [DID module (0x22 / 0x2E / 0x2C)](#10-did-module-0x22--0x2e--0x2c)
11. [Doing something after the response is sent](#11-doing-something-after-the-response-is-sent)
12. [Request pipeline and NRC reference](#12-request-pipeline-and-nrc-reference)
13. [Memory and code size](#13-memory-and-code-size)
14. [Threading rules](#14-threading-rules)
15. [Troubleshooting](#15-troubleshooting)
16. [Client (planned)](#16-client-planned)
17. [Further reading and license](#17-further-reading-and-license)

---

## 1. Status

| Area | State |
|---|---|
| `0x10` DiagnosticSessionControl, `0x27` SecurityAccess, `0x3E` TesterPresent | Implemented (built-in) |
| `0x22` ReadDataByIdentifier, `0x2E` WriteDataByIdentifier, `0x2C` DynamicallyDefineDataIdentifier | Implemented (DID module) |
| Your own services with sub-function tables | Implemented |
| Session timeout (S3), `0x78` ResponsePending, suppress-positive-response bit, functional-addressing rules | Implemented |
| Post-response hook (reset / jump to bootloader) | Implemented |
| `0x31` RoutineControl, `0x11` ECUReset, `0x14` `0x19` DTC, `0x28`, `0x85`, `0x34`–`0x37` | **Not built in** — register them yourself (see [section 9](#9-writing-your-own-services) and `example/Server`) |
| Client (tester side) | **Planned** — see [section 16](#16-client-planned) |
| Multiple server instances | Not supported (single instance) |
| OEM-specific session IDs above 31 (e.g. `0x40`) | Not supported (sessions are bit-masks, IDs 1..31) |
| Variable-length DIDs | Not supported yet (every DID has a fixed length) |

The library compiles warning-free with `-std=c99 -Wall -Wextra -Wpedantic -Wshadow -Wconversion`.

---

## 2. Repository layout

```
MicroUds/
├── MicroUds_conf.h        # configuration (edit this)
├── MicroUds_types.h       # common types: status codes, NRCs, sessions, service flags
├── MicroUds_utils.h       # helper macros (session masks, permission bytes, tick math)
├── CMakeLists.txt
├── Server/
│   ├── include/
│   │   ├── MicroUdsServer.h            # public API  <- include this
│   │   ├── MicroUdsServer_types.h      # service tables, callback context, kernel state
│   │   ├── MicroUdsServer_Did.h        # DID module API
│   │   └── MicroUdsServer_Did_types.h  # DID tables and DID callback context
│   └── src/
│       ├── MicroUdsServer.c
│       └── MicroUdsServer_Did.c
├── Client/                # reserved (not implemented yet)
├── example/
│   ├── Server/            # runnable PC example  <- start here
│   └── Client/            # reserved
└── doc/                   # design notes (Chinese)
```

---

## 3. How it fits together

```
 Your application         services, DID tables, security algorithm, hooks
        │  ▲
        ▼  │   MicroUdsServer_ServiceRegisterServ / _Did_RegisterReadItem / callbacks
 ┌────────────────────────────────────────────────────────────┐
 │ MicroUds Server                                            │
 │   dispatch pipeline · sessions · security · S3 · 0x78      │
 │   response queue · transmit-confirm · DID module           │
 └────────────────────────────────────────────────────────────┘
        │  ▲
        ▼  │   RespTransmit()  /  RxIndication()  /  HwRespTxDone()
 ISO-TP stack (MicroIsotp or any other)  ──  CAN driver
```

Who calls what:

| Function | Who calls it | From where |
|---|---|---|
| `MicroUdsServer_Init()` | you | once at start-up, **before** registering anything |
| `MicroUdsServer_TickHandler()` | you | timer interrupt, at `MICROUDS_FREQ_HZ` |
| `MicroUdsServer_TimerHandler()` | you | main loop / task (never from an ISR) |
| `MicroUdsServer_RxIndication()` | ISO-TP | "complete message received" callback (ISR or task) |
| `MicroUdsServer_HwRespTxDone()` | ISO-TP | "message sent" callback (ISR or task) |
| `MicroUdsServer_RespTransmit()` | the kernel | **you implement it** — hand the PDU to ISO-TP |

---

## 4. Quick start

### 4.1 Add the library to your build

**CMake**

```cmake
add_library(microuds STATIC
    MicroUds/Server/src/MicroUdsServer.c
    MicroUds/Server/src/MicroUdsServer_Did.c)

target_include_directories(microuds PUBLIC
    MicroUds                      # MicroUds_conf.h, MicroUds_types.h, MicroUds_utils.h
    MicroUds/Server/include)      # MicroUdsServer*.h

target_link_libraries(your_app PRIVATE microuds)
```

**Keil / IAR / Makefile** — add the two `.c` files and the two include directories above. The code needs C99, `stdint.h`, `stdbool.h`, and a compiler that supports `__attribute__((weak))`.

> **Weak symbols.** `RespTransmit`, `SecurityReqSeed`, `SecurityGetKey`, `OnResponseSent` and `Did_AddrReadable` are weak functions with harmless defaults. Define your own version in your *application* sources. If you build MicroUds as a **static library**, do not put your overrides in another static library that nothing else references — the linker will not pull that object in and the weak default silently wins.

### 4.2 Minimal integration

```c
#include "MicroUdsServer.h"

/* 1. Transmit: hand the response to your ISO-TP stack. Must not block. */
MicroUds_Status_t MicroUdsServer_RespTransmit(const uint8_t *pdu, uint32_t len)
{
    if (!isotp_is_idle())            return MICROUDS_BUSY;   /* the kernel retries next cycle */
    isotp_send(pdu, len);            /* your ISO-TP API */
    return MICROUDS_OK;
}

/* 2. ISO-TP callbacks */
void isotp_on_message_sent(void)                     { MicroUdsServer_HwRespTxDone(); }
void isotp_on_message_received(const uint8_t *d, size_t n, bool functional)
{
    MicroUdsServer_RxIndication(d, n, functional ? MICROUDS_ADDR_FUNC : MICROUDS_ADDR_PHY);
}

/* 3. Tick: call at MICROUDS_FREQ_HZ (default 1 kHz) */
void SysTick_Handler(void) { MicroUdsServer_TickHandler(); }

int main(void)
{
    MicroUdsServer_Init();                                   /* always first */
    MicroUdsServer_ServiceRegisterServ(my_services, MICROUDS_SIZEOF(my_services));
    MicroUdsServer_Did_RegisterReadItem(my_read_dids, MICROUDS_SIZEOF(my_read_dids));
    MicroUdsServer_Did_RegisterWriteItem(my_write_dids, MICROUDS_SIZEOF(my_write_dids));

    for (;;)
    {
        MicroUdsServer_TimerHandler();                       /* drives everything */
        /* ... the rest of your main loop ... */
    }
}
```

That is enough to answer `10 01/02/03`, `3E 00`, `27 xx` (once you implement the two security functions, see [8.2](#82-0x27-securityaccess)), and every DID you registered.

### 4.3 Run the example (no hardware needed)

`example/Server/main.c` simulates an ISO-TP stack on a PC, sends scripted requests and prints every response.

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

Excerpt of the output:

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

## 5. Porting guide

### Functions you call

| Function | Notes |
|---|---|
| `MicroUds_Status_t MicroUdsServer_Init(void)` | Resets the kernel and registers the built-in services. Anything you registered earlier is discarded. |
| `void MicroUdsServer_TickHandler(void)` | Increments the internal tick. Call at exactly `MICROUDS_FREQ_HZ`. |
| `void MicroUdsServer_TimerHandler(void)` | The kernel main function. Each call advances the S3 timer, may dispatch one request and may advance one response step. Call it every millisecond or at least every few milliseconds (the advertised P2 is 50 ms). |
| `MicroUds_Status_t MicroUdsServer_RxIndication(const uint8_t *data, size_t len, MicroUds_AddrType_t addr)` | Feed one **complete** request PDU (SID first). The data is copied, so your buffer can be reused immediately. Returns `MICROUDS_OK`; `MICROUDS_BUSY` if the previous request is still in progress (including *waiting for its response to be sent*) — the request is dropped, you decide whether to retry or answer `0x21`; `MICROUDS_PARAM_ERR` for a null pointer or zero length. |
| `void MicroUdsServer_HwRespTxDone(void)` | Tell the kernel the response given to `RespTransmit` has been sent (or failed). Only sets a flag, so it is safe in an ISR. |
| `uint8_t MicroUdsServer_GetSession(void)` | Current session number. |
| `uint8_t MicroUdsServer_GetSecurityLevel(void)` | Currently unlocked level, `0` = locked. |

`addr` is `MICROUDS_ADDR_PHY` (physical) or `MICROUDS_ADDR_FUNC` (functional). The kernel needs it because the two have different response rules (see [7.4](#74-physical-vs-functional-addressing)).

### Functions you implement (weak)

| Function | Required? | Purpose |
|---|---|---|
| `MicroUds_Status_t MicroUdsServer_RespTransmit(const uint8_t *pdu, uint32_t len)` | **Yes** | Give the response to ISO-TP. Return `MICROUDS_OK` when accepted (then call `HwRespTxDone` when it is on the wire), `MICROUDS_BUSY` if the transport cannot take it now (the kernel retries next cycle), anything else to drop the response. The default does nothing and returns OK, so **forgetting this produces silence, not an error**. |
| `void MicroUdsServer_SecurityReqSeed(uint8_t level, uint8_t *out, uint32_t *len)` | If you use `0x27` | Generate a seed. `*len` is the buffer capacity (`MICROUDS_SEED_SIZE`) on entry; set it to the number of bytes written. Setting `0` means "failed". The default sets `0`, so `27 01` answers `0x10` until you implement it. |
| `void MicroUdsServer_SecurityGetKey(uint8_t level, const uint8_t *seed, uint32_t seed_len, uint8_t *key, uint32_t *key_len)` | If you use `0x27` | Compute the key the tester must send for that seed. `*key_len` is the capacity (`MICROUDS_KEY_SIZE`) on entry; set it to the key length. The default sets `0`, so every key is rejected (safe default). |
| `void MicroUdsServer_OnResponseSent(MicroUdsServer_ServiceCtx_t *ctx)` | Optional | See [section 11](#11-doing-something-after-the-response-is-sent). |
| `bool MicroUdsServer_Did_AddrReadable(uint32_t addr, uint32_t len)` | Only if `MICROUDS_DID_DYNA_BY_ADDR = 1` | Whitelist for `0x2C 02`. The default denies everything. |

Seeds must come from a hardware RNG and the key algorithm should be a real MAC (e.g. AES-CMAC). The XOR in the example is for demonstration only.

---

## 6. Configuration reference

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
| `MICROUDS_PENDING_MAX_COUNT` | `10` | After this many `0x78` the kernel gives up and answers `0x10`. |
| `MICROUDS_TX_CONFIRM_TIMEOUT_MS` | `1000` | Maximum wait for `HwRespTxDone` after `RespTransmit` returned OK; afterwards the response is considered finished. |
| `MICROUDS_REQUEST_BUF_SIZE` | `64` | Largest request (full PDU, bytes). Longer requests are answered `0x13`. |
| `MICROUDS_RESPONSE_BUF_SIZE` | `64` | Largest response (full PDU, bytes). |
| `MICROUDS_SESSION_MASK` | `4` | **Highest** session number supported: `4` means sessions 1, 2, 3, 4. |
| `MICROUDS_SEED_SIZE` / `MICROUDS_KEY_SIZE` | `4` / `4` | Security seed and key length. |
| `MICROUDS_SECURITY_MAX_LEVEL` | `4` | Number of security levels (`27 01/02` = level 1 … ). |
| `MICROUDS_SECURITY_MAX_ATTEMPTS` | `3` | Failed keys before lock-out. |
| `MICROUDS_SECURITY_DELAY_MS` | `10000` | Lock-out time after too many failures. |
| `MICROUDS_SERVICE_MAX_NUMBER` | `16` | Size of the service table. **Built-in services use 3 (6 with the DID module)**, leaving 10 for yours. |
| `MICROUDS_SUBFUNC_MAX_NUMBER` | `3` | Sub-functions per service table entry. |
| `MICROUDS_DID_ENABLE` | `1` | Build the DID module. |
| `MICROUDS_DID_READ_SIZE` / `MICROUDS_DID_WRITE_SIZE` | `8` / `8` | Capacity of the read / write DID tables. |
| `MICROUDS_DID_DYNAMICALLY_SIZE` | `8` | Number of dynamic DIDs. |
| `MICROUDS_DID_DYNAMICALLY_ITEM_LEN` | `5` | Segments per dynamic DID. |
| `MICROUDS_DID_READ_MAX_PER_REQ` | `8` | Most DIDs in one `0x22`; more is answered `0x31`. |
| `MICROUDS_DID_READ_PARTIAL` | `0` | `0` = if any DID of a `0x22` is unknown/unavailable, the whole request fails. `1` = skip those, answer the rest. |
| `MICROUDS_DID_NRC_SESSION` | `0x31` | NRC when a DID exists but is not available in the current session. |
| `MICROUDS_DID_DYNA_ID_MIN` / `_MAX` | `0xF300` / `0xF3FF` | DID range allowed for dynamic DIDs. |
| `MICROUDS_DID_DYNA_BY_ADDR` | `1` | Allow `0x2C 02` (dynamic DID by memory address). **Exposes memory to the tester — keep the whitelist tight, or set `0`.** |
| `MICROUDS_DID_DYNA_CLEAR_ON_DEFAULT` | `1` | Delete all dynamic DIDs when the server returns to the default session. |
| `MICROUDS_DID_READ_REQ_MIN_LEN` / `WRITE_REQ_MIN_LEN` / `DYNA_REQ_MIN_LEN` | `3` / `4` / `2` | Minimum request lengths used by the built-in DID services. Normally not changed. |
| `MICROUDS_DID_RESP_MAX_LEN` / `RESP_MIN_LEN` | `32` / `2` | Unused (kept for compatibility). |

---

## 7. Core concepts

### 7.1 One request at a time

The kernel owns one request buffer and one response buffer. A request is "in progress" from `RxIndication` until its **final response has been sent** (`HwRespTxDone`), so a new request arriving earlier gets `MICROUDS_BUSY`. A `0x78` ResponsePending is not a final response: the request stays alive after it is sent.

### 7.2 Sessions

Session numbers are the values used in `10 xx`:

| Constant | Value | Typical use |
|---|---|---|
| `MICROUDS_SESSION_DEFAULT` | 1 | Power-on session |
| `MICROUDS_SESSION_PROGRAM` | 2 | Programming |
| `MICROUDS_SESSION_EXTEND` | 3 | Extended diagnostics |
| `MICROUDS_SESSION_SECURITY` | 4 | Safety-system diagnostics |

Configuration fields that say "session mask" use one bit per session:

```c
uint32_t mask = MICROUDS_SET_SESSION(MICROUDS_SESSION_DEFAULT) |
                MICROUDS_SET_SESSION(MICROUDS_SESSION_EXTEND);
```

Rules implemented by the kernel:

- The server powers up in the default session. A `10 xx` is answered first; the session changes **after the response has been sent**.
- Whenever the session actually changes, **security is locked again** (the failed-attempt counter is not reset), and — with `MICROUDS_DID_DYNA_CLEAR_ON_DEFAULT` — dynamic DIDs are deleted when going back to the default session.
- Requesting the session you are already in does not lock security (OEM specifications differ here; change `MicroUdsServer_SessionSwitch` if yours says otherwise).
- **S3**: in a non-default session, if nothing happens for `MICROUDS_SESSION_HOLD_MS`, the server returns to the default session. The timer restarts on every received request and when a request finishes, and does not run while a request is in progress.
- P2/P2\* are only *reported* in the `0x10` response. The kernel does not time P2; for anything that may take longer than P2, return **pending** (section 9.3).

### 7.3 Security levels and permission bytes

`27 01/02` unlocks level 1, `27 03/04` level 2, and so on. **Only one level is active at a time**: unlocking another level replaces the current one.

A *permission byte* (`permis`, used in service tables, sub-function tables and DID tables) says which unlocked levels may use the item:

| Value | Helper | Meaning |
|---|---|---|
| `0` | — | No unlock needed |
| `n` | `MICROUDS_PERMIS_ATLEAST(n)` | Current level must be **≥ n** |
| `0x80 \| n` | `MICROUDS_PERMIS_EXACT(n)` | Current level must be **exactly n** |

### 7.4 Physical vs functional addressing

- A service accepts functional requests only if it has `MICROUDS_SERV_FLAG_FUNC_ADDR`; otherwise the request is silently ignored.
- Built-in: `0x10` and `0x3E` accept functional addressing. `0x27`, `0x22`, `0x2E`, `0x2C` do not.
- For functional requests, negative responses with NRC `0x11 0x12 0x31 0x7E 0x7F` are **not sent** (so a broadcast does not make every ECU answer "not supported"). Positive responses are sent normally.

### 7.5 Suppress-positive-response

If a service has `MICROUDS_SERV_FLAG_SUPPRESS` and the request's sub-function has bit 7 set (`3E 80`, `10 83`, `11 81` …), the positive response is not sent. Negative responses are always sent. The service still runs, and `OnResponseSent` is still called. Sub-function numbers of such services must be below `0x80`.

---

## 8. Built-in services

| Service | Sessions | Functional | Suppress | Request length | Behaviour |
|---|---|---|---|---|---|
| `0x10` | all supported | yes | yes | exactly 2 | Positive: `50 <session> <P2 hi lo> <P2* hi lo>` (P2\* in 10 ms units). Unsupported session → `0x12`. |
| `0x27` | Programming, Extended | no | no | ≥ 2 | See below. |
| `0x3E` | all supported | yes | yes | exactly 2 | Only sub-function `00`; others → `0x12`. |
| `0x22` `0x2E` `0x2C` | see [section 10](#10-did-module-0x22--0x2e--0x2c) | | | | DID module |

### 8.1 `0x10` DiagnosticSessionControl

Nothing to configure. The supported sessions are `1 … MICROUDS_SESSION_MASK`.

### 8.2 `0x27` SecurityAccess

- Odd sub-function = requestSeed, the next even number = sendKey (`27 01` / `27 02` for level 1).
- Valid sub-functions: `1 … 2 × MICROUDS_SECURITY_MAX_LEVEL`; otherwise `0x12`.
- requestSeed: if the delay timer is running → `0x37`; if that level is already unlocked → an all-zero seed; otherwise your `SecurityReqSeed` is called (if it reports length 0 → `0x10`).
- sendKey must directly follow a requestSeed **for the same level**, otherwise `0x24`. Key length must be `MICROUDS_KEY_SIZE`, otherwise `0x13` (the seed stays valid). Your `SecurityGetKey` computes the expected key; wrong key → `0x35`.
- A seed is single use: after any sendKey it is discarded, success or not.
- After `MICROUDS_SECURITY_MAX_ATTEMPTS` consecutive failures the answer is `0x36` and requestSeed answers `0x37` for `MICROUDS_SECURITY_DELAY_MS`. Session changes and S3 timeouts do **not** reset the counter.

---

## 9. Writing your own services

### 9.1 The service table

```c
static const MicroUdsServer_ServiceTable_t my_services[] = {
    {
        .sid          = 0x11,                                          /* service ID */
        .flags        = MICROUDS_SERV_FLAG_HAS_SUBFUNC |               /* byte 2 is a sub-function  */
                        MICROUDS_SERV_FLAG_SUPPRESS,                   /* sub-function bit 7 = suppress */
        .permis       = 0,                                             /* security requirement (7.3) */
        .session_mask = MICROUDS_SET_SESSION(MICROUDS_SESSION_EXTEND) |
                        MICROUDS_SET_SESSION(MICROUDS_SESSION_PROGRAM),
        .min_len      = 2,                                             /* full PDU length incl. SID, 0 = unchecked */
        .max_len      = 2,
        .callback     = svc_ecu_reset,
        .Userdata     = NULL,
    },
};

MicroUdsServer_ServiceRegisterServ(my_services, MICROUDS_SIZEOF(my_services));   /* after Init */
```

| Field | Meaning |
|---|---|
| `sid` | Service ID. Must not collide with a built-in one or another entry. |
| `flags` | `HAS_SUBFUNC`: the second request byte is a sub-function — the kernel parses it, handles the suppress bit and **echoes it in the positive response**. `SUPPRESS`: the sub-function's bit 7 is the suppress bit (requires `HAS_SUBFUNC`). `FUNC_ADDR`: accept functional addressing. Do not set `BUILTIN`. |
| `permis` | Security requirement for the whole service. |
| `session_mask` | Sessions in which the service exists. Anything else → `0x7F`. A value of `0` means "never available" (`0x11`). |
| `min_len` / `max_len` | Full PDU length including the SID. Violations → `0x13`. |
| `callback` | Called when the request passed all checks. May be `NULL` only if you register a sub-function table. |
| `Userdata` | Passed back as `ctx->Userdata`. |

`MicroUdsServer_ServiceRegisterServ` is **atomic** (nothing is registered if any entry is invalid) and can be called several times. It returns `MICROUDS_PARAM_ERR` for a null/empty table, a full service table, a duplicate SID, the `BUILTIN` flag, or `SUPPRESS` without `HAS_SUBFUNC`.

### 9.2 The callback context

```c
static void svc_ecu_reset(MicroUdsServer_ServiceCtx_t *ctx)
{
    if (ctx->sub_func == 0x01)
    {
        MicroUdsServer_CtxPositive(ctx, 0);                 /* answers 51 01 */
        return;
    }
    MicroUdsServer_CtxNegative(ctx, MICROUDS_NRC_SFNS);     /* answers 7F 11 12 */
}
```

**You must always set a result.** The context starts as "negative, `0x10`"; call exactly one of:

| Helper | Result |
|---|---|
| `MicroUdsServer_CtxPositive(ctx, len)` | Positive response. The `len` bytes you wrote to `ctx->payload` are appended after the SID (and sub-function). |
| `MicroUdsServer_CtxNegative(ctx, nrc)` | Negative response `7F <sid> <nrc>`. |
| `MicroUdsServer_CtxPending(ctx)` | Not finished yet — see 9.3. |

Inputs (read-only):

| Field | Meaning |
|---|---|
| `sid`, `sub_func` | Service ID and sub-function (suppress bit already removed; `0` if the service has none). |
| `req`, `req_len` | Request bytes **after** the SID (and after the sub-function if `HAS_SUBFUNC`). |
| `addr` | `MICROUDS_ADDR_PHY` or `_FUNC`. |
| `session`, `sec_level` | Current session number and unlocked level. |
| `call_cnt` | `0` on the first call, then 1, 2, … on each re-entry after *pending*. |
| `Userdata` | From the table entry. |

Outputs: `payload` points into the response buffer (already past the SID/sub-function), `payload_cap` is the space left; write there, then call `CtxPositive` with the length. If the answer would not fit, the kernel replies `0x14`.

> Your callbacks run inside `TimerHandler`. Keep them short and non-blocking.

### 9.3 Long operations: *pending*

```c
static void rc_erase(MicroUdsServer_ServiceCtx_t *ctx)
{
    if (!flash_erase_done())
    {
        MicroUdsServer_CtxPending(ctx);          /* kernel sends 7F <sid> 78 immediately */
        return;                                  /* ...and calls this function again every cycle */
    }
    MicroUdsServer_CtxPositive(ctx, 0);          /* finally: the real response */
}
```

- The first `CtxPending` sends `0x78` at once. The callback is then called again on every `TimerHandler` pass (`ctx->call_cnt` increases) until it returns positive or negative.
- Start the real work when `call_cnt == 0`; keep state in your own variables (the context is rebuilt each call).
- `0x78` is repeated every `MICROUDS_PENDING_RESEND_MS`; after `MICROUDS_PENDING_MAX_COUNT` repeats the kernel ends the request with `0x10`.
- New requests get `BUSY` while pending.

### 9.4 Sub-function tables

If every sub-function has its own handler or its own permissions, register a table (service entry with `.callback = NULL` or a default handler):

```c
static const MicroUdsServer_SubFuncTable_t rc_subs[] = {
    { .id = 0x01, .callback = rc_start,   .permis = MICROUDS_PERMIS_ATLEAST(1) },  /* needs level 1 */
    { .id = 0x02, .callback = rc_stop    },
    { .id = 0x03, .callback = rc_results },
};
MicroUdsServer_ServiceRegisterSubFunc(0x31, rc_subs, MICROUDS_SIZEOF(rc_subs));
```

| Field | Meaning |
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

## 10. DID module (0x22 / 0x2E / 0x2C)

### 10.1 Defining DIDs

Read and write tables are separate. **A DID that is not in the write table is read-only.** The same DID may appear in both with different permissions.

```c
static uint8_t g_vin[17] = { 'M','I','C','R','O','U','D','S','0','0','0','0','0','0','0','0','1' };
static uint8_t g_cfg[2];

static const MicroUdsServerDidItem_Config_t read_dids[] = {
    { .Identifier = 0xF190, .data = g_vin, .data_len = sizeof g_vin },                 /* all sessions, no unlock */
    { .Identifier = 0x0100, .data = g_cfg, .data_len = sizeof g_cfg,
      .session = MICROUDS_SET_SESSION(MICROUDS_SESSION_EXTEND) },                      /* extended only */
};
static const MicroUdsServerDidItem_Config_t write_dids[] = {
    { .Identifier = 0x0100, .data = g_cfg, .data_len = sizeof g_cfg,
      .session = MICROUDS_SET_SESSION(MICROUDS_SESSION_EXTEND),
      .permis  = MICROUDS_PERMIS_ATLEAST(1) },                                         /* extended + unlocked */
};

MicroUdsServer_Did_RegisterReadItem (read_dids,  MICROUDS_SIZEOF(read_dids));   /* after Init */
MicroUdsServer_Did_RegisterWriteItem(write_dids, MICROUDS_SIZEOF(write_dids));
```

| Field | Meaning |
|---|---|
| `Identifier` | 16-bit DID. |
| `session` | Session mask where the DID is available. **`0` = all sessions.** |
| `permis` | Security requirement, see 7.3. **`0` = no unlock.** |
| `data` / `data_len` | Your variable and its size. DIDs have a **fixed length**. |
| `cb` | Optional callback, see 10.2. |
| `Userdata` | Passed to the callback as `ctx->Userdata`. |

> `session = 0` and `permis = 0` mean *no restriction*. A forgotten field opens the DID, so set both explicitly for anything sensitive.

Registration can be called several times (tables are appended) and is atomic. It returns `MICROUDS_PARAM_ERR` for: empty table, no room left, duplicate DID, `data_len == 0`, a **read** item with `data == NULL`, a read item longer than `MICROUDS_RESPONSE_BUF_SIZE − 3`, a **write** item with neither `data` nor `cb`, or a write item that does not fit in the request buffer.

The access check is: *service* conditions (the built-in `0x22`/`0x2E` are open in all sessions with no unlock) **and** the DID's own `session`/`permis`. The real policy lives on each DID.

### 10.2 DID callbacks

The callback is invoked **before** the kernel copies any data.

```c
typedef struct {
    /* inputs */
    uint16_t Did;  uint16_t Did_len;          /* the DID; number of DIDs in this request */
    uint8_t *data; uint16_t data_len;         /* your variable (may be NULL for write-only items) */
    void *Userdata;
    bool is_write;
    const uint8_t *in_data; uint16_t in_len;  /* write: the bytes sent by the tester (read-only) */
    uint8_t session, sec_level;
    uint32_t call_cnt;                        /* write + pending: 0 first, then 1, 2 ... */
    /* outputs */
    uint8_t nrc;                              /* set to reject */
    bool pending;                             /* write only: not finished yet */
} MicroUdsServerDid_Ctx_t;
```

- **Read**: refresh `ctx->data` with the current value (e.g. a sensor). The kernel then copies `data` into the response. Set `ctx->nrc` to reject. Pending is not supported for reads (answers `0x10`).
- **Write**: validate `ctx->in_data` (range, state …). If you leave `nrc == 0` the write is **accepted** and the kernel copies `in_data` into `data` (if non-`NULL`). If you set `nrc`, nothing is changed. If your variable is not plain RAM (e.g. NVM), pass `data = NULL` and store the bytes yourself in the callback.
- **Slow writes**: set `ctx->pending = true`. The kernel answers `0x78`, calls the callback again every cycle (`call_cnt` increments) and accepts the write when it returns without `pending`.

### 10.3 `0x22` ReadDataByIdentifier

`22 <DID> [<DID> ...]` → `62 <DID> <data> [<DID> <data> ...]`

- Request length must be `1 + 2N` (else `0x13`), with `N ≤ MICROUDS_DID_READ_MAX_PER_REQ` (else `0x31`).
- **Two phases**: first every DID is checked (exists, session, security, total length) *without* reading anything, then the data is read. So a bad DID never leaves you with half-executed read callbacks.
- Unknown DID, or DID not available in this session → `0x31` (the session NRC is `MICROUDS_DID_NRC_SESSION`). With `MICROUDS_DID_READ_PARTIAL = 1` such DIDs are skipped and the rest answered (`0x31` only if nothing is left).
- Security not satisfied for **any** DID → `0x33` for the whole request, never partial data.
- Response too large for `MICROUDS_RESPONSE_BUF_SIZE` → `0x14`.
- Static and dynamic DIDs can be mixed in one request.

### 10.4 `0x2E` WriteDataByIdentifier

`2E <DID> <data>` → `6E <DID>`

- The DID must be in the write table, otherwise `0x31` (this also protects dynamic DIDs, which can never be written).
- Order of checks: DID exists → session/security (`0x31` / `0x33`) → length must equal `data_len` exactly (`0x13`) → your callback.

### 10.5 `0x2C` DynamicallyDefineDataIdentifier

Lets the tester build a new DID out of pieces of existing ones. Available in the Programming and Extended sessions.

| Sub-function | Request | Notes |
|---|---|---|
| `01` define by identifier | `2C 01 <DDDID> { <srcDID> <pos> <size> } × N` | `pos` starts at **1**. Up to `MICROUDS_DID_DYNAMICALLY_ITEM_LEN` segments. |
| `02` define by address | `2C 02 <DDDID> <ALFID> { <addr> <size> } × N` | Only with `MICROUDS_DID_DYNA_BY_ADDR = 1`. ALFID must be `0x24` or `0x14`-style: **4-byte address**, 1- or 2-byte size. Otherwise `0x12`/`0x31`. |
| `03` clear | `2C 03` or `2C 03 <DDDID>` | Without a DID: clear all. Unknown DID → `0x31`. |

Positive response: `6C <sub> <DDDID>` (`6C 03` for clear-all).

Rules:

- `DDDID` must be inside `MICROUDS_DID_DYNA_ID_MIN … MAX` and must not be a static DID.
- Every source DID must be readable *now* (session + security). The check is **repeated every time the dynamic DID is read**, so a dynamic DID can never be used to get around a source DID's protection.
- Total length must fit in the response buffer. Defining an existing `DDDID` again **replaces** it. A failed definition changes nothing.
- All dynamic DIDs are deleted when returning to the default session (`MICROUDS_DID_DYNA_CLEAR_ON_DEFAULT`).
- Sub-function `02` reads **raw memory**. The only protection is your `MicroUdsServer_Did_AddrReadable()` whitelist, which denies everything by default, and the Programming/Extended-session requirement; the current implementation does not ask for a security unlock. Set `MICROUDS_DID_DYNA_BY_ADDR` to `0` if you do not need it.

```c
bool MicroUdsServer_Did_AddrReadable(uint32_t addr, uint32_t len)
{
    return (addr >= 0x20000000u) && (len <= 0x100u) && ((addr - 0x20000000u) <= (0x100u - len));
}
```

---

## 11. Doing something after the response is sent

Resetting the ECU or jumping to the bootloader must happen **after** the positive response has left the bus. Implement one hook for all services (built-in and yours):

```c
void MicroUdsServer_OnResponseSent(MicroUdsServer_ServiceCtx_t *ctx)
{
    if (ctx->resp_type != MICROUDS_RESPTYPE_POSITIVE)      /* IMPORTANT: negative responses also arrive here */
    {
        return;
    }
    if (ctx->sid == 0x11 && ctx->sub_func == 0x01) { NVIC_SystemReset(); }
    if (ctx->sid == 0x10 && ctx->sub_func == 0x02) { jump_to_bootloader(); }
}
```

When it is called:

| Situation | Hook called? |
|---|---|
| A service callback ran and its final response (positive **or negative**) was sent | yes |
| The positive response was suppressed (`11 81`) | yes — the reset still has to happen |
| `0x78` ResponsePending | no |
| The request was rejected by the pipeline before reaching a service (unknown SID, wrong session, bad length, security denied …) | no |
| A functional request that was silently ignored | no |
| The transport never confirmed (`MICROUDS_TX_CONFIRM_TIMEOUT_MS`) or `RespTransmit` failed | yes — the hook cannot tell whether the bytes really left |

Inside the hook:

- It runs in `TimerHandler` context. It may never return (reset / jump). If it returns, the request is released afterwards. New requests get `BUSY` while it runs.
- `ctx->session` is already the **new** session after a `10 xx`.
- Valid `ctx` fields: `sid`, `sub_func`, `addr`, `req`/`req_len`, `session`, `sec_level`, `call_cnt` (how many times the service callback ran), `Userdata`, `resp_type` (positive or negative), `nrc` (when negative). `payload`, `payload_cap`, `len` are not used.

---

## 12. Request pipeline and NRC reference

Every request goes through these checks, in this order. The first failure ends the request with the listed NRC.

| # | Check | NRC |
|---|---|---|
| 1 | Request longer than `MICROUDS_REQUEST_BUF_SIZE` | `0x13` |
| 2 | Service not registered | `0x11` |
| 3 | Functional request and service lacks `FUNC_ADDR` | *ignored, no response* |
| 4 | Service not available in the current session | `0x7F` |
| 5 | `HAS_SUBFUNC` but the request has no sub-function byte | `0x13` |
| 6 | Sub-function table registered and sub-function missing / not in this session | `0x12` / `0x7E` |
| 7 | Request length outside `min_len … max_len` | `0x13` |
| 8 | Service or sub-function security not satisfied | `0x33` |
| 9 | Your callback runs | whatever it returns |

NRCs produced by the kernel and modules:

| NRC | Meaning | Produced by |
|---|---|---|
| `0x10` | General reject | callback without `NULL` check, seed/key not implemented, too many `0x78`, a result never set |
| `0x11` | Service not supported | pipeline |
| `0x12` | Sub-function not supported | pipeline, `0x10`, `0x27`, `0x3E`, `0x2C` |
| `0x13` | Wrong length / format | pipeline, `0x27`, DID services |
| `0x14` | Response too long | response builder, `0x22`, `0x27` |
| `0x24` | Request sequence error | `0x27` sendKey without a matching seed |
| `0x31` | Request out of range | DID services (unknown DID, DID not in this session, bad dynamic definition …) |
| `0x33` | Security access denied | pipeline, DID services |
| `0x35` `0x36` `0x37` | Invalid key / too many attempts / delay running | `0x27` |
| `0x78` | Response pending | kernel, when a callback returns *pending* |
| `0x7E` `0x7F` | Sub-function / service not supported in the active session | pipeline |

---

## 13. Memory and code size

Measured with the default configuration, 32-bit pointers (`sizeof` read from a `-m32` build):

| Item | RAM |
|---|---:|
| Kernel state (`MicroUdsServer_Obj_t`) | 1744 B |
| DID module state | 1000 B |
| **Total** | **≈ 2.7 KB** |

How it scales (32-bit): each service table entry 92 B (× `MICROUDS_SERVICE_MAX_NUMBER`), each sub-function entry 20 B (× `SUBFUNC_MAX_NUMBER` per service), request buffer = `MICROUDS_REQUEST_BUF_SIZE`, response buffer = `MICROUDS_RESPONSE_BUF_SIZE` (plus small headers), each DID item 24 B, each dynamic DID 76 B.

Code size is about 9.4 KB at `-Os` (measured for x86-32; use your own toolchain for a real number). Setting `MICROUDS_DID_ENABLE` to `0` removes the DID module.

No dynamic allocation, no recursion, no floating point.

---

## 14. Threading rules

- `MicroUdsServer_TimerHandler()` must always be called from the **same context** (main loop or one task), never from an ISR.
- `MicroUdsServer_RxIndication()` and `MicroUdsServer_HwRespTxDone()` may be called from an ISR or another task, but not re-entrantly with themselves. They only copy the request / set a flag.
- `MicroUdsServer_TickHandler()` is meant for a timer ISR.
- Registration functions and `Init` must run before the first request arrives; do not call them while `TimerHandler` is running.
- Your callbacks (service, DID, hook, `RespTransmit`) run inside `TimerHandler`. Do not block in them.

---

## 15. Troubleshooting

| Symptom | Likely cause |
|---|---|
| The ECU never answers | `MicroUdsServer_RespTransmit` is not being overridden (misspelled name, or the override is in an unreferenced static library); `TimerHandler` is not being called; `TickHandler` is not running. |
| Every request returns `MICROUDS_BUSY` | The previous response was never confirmed: call `MicroUdsServer_HwRespTxDone()` when ISO-TP finishes sending. Without it the kernel only recovers after `MICROUDS_TX_CONFIRM_TIMEOUT_MS`. |
| A `Register…` function returns `MICROUDS_PARAM_ERR` | See sections 9.1 and 10.1 for the exact list. A common one: registering after a second `Init`, or a duplicate SID/DID. |
| Everything I registered disappeared | `MicroUdsServer_Init()` resets everything. Register after `Init`, every time. |
| `27 01` answers `7F 27 10` | `MicroUdsServer_SecurityReqSeed` is not implemented (the default reports length 0). |
| `27 02` always answers `35` | `MicroUdsServer_SecurityGetKey` is not implemented, or your algorithm differs from the tester's. |
| `22 xx xx` answers `0x31` in a session where it should work | The DID's `session` mask does not include the current session. Remember `0` means *all*. |
| A request answered `0x7F` | The *service's* `session_mask` does not include the current session (this is different from a DID's session, which gives `0x31`). |
| My reset runs even when the tester was rejected | The hook is also called for negative responses; test `ctx->resp_type == MICROUDS_RESPTYPE_POSITIVE`. |
| Long DIDs fail with `0x14` | Increase `MICROUDS_RESPONSE_BUF_SIZE`. |
| Requests longer than 64 bytes fail with `0x13` | Increase `MICROUDS_REQUEST_BUF_SIZE`. |
| Duplicate-symbol link errors for the weak functions | Your toolchain handles weak symbols differently; confirm that a strong definition overrides a weak one on your target (the library is tested with GCC). |

---

## 16. Client (planned)

The tester-side client is **not implemented yet**. The following is reserved:

- source directories `Client/include` and `Client/src`, and the switch `MICROUDS_CLIENT_ENABLE` (currently `0`);
- `example/Client/` for a runnable client example.

Intended scope: build requests, parse positive/negative responses (including `0x78`), P2/P2\* timeouts and TesterPresent keep-alive. The API will be documented in this section when it exists.

---

## 17. Further reading and license

- `doc/` — design notes (in Chinese): the UDS architecture notes and the DID knowledge document.
- `example/Server/main.c` — the reference for everything in this manual.
- License: see [LICENSE](LICENSE).
