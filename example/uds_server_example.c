/**
 * @file main.c
 * @brief MicroUds Server 示例 / MicroUds Server example
 *
 * 在 PC 上直接运行，不需要任何硬件: 用一个"模拟的 ISO-TP"把请求喂给内核，并把内核发出的响应打印出来。
 * Runs on a PC with no hardware: a simulated ISO-TP feeds requests into the kernel and prints every response.
 *
 * 文件分为 6 节 / The file has 6 sections:
 *   1. 移植层   Port layer          换到 MCU 时替换这一节 / replace this section on a real MCU
 *   2. 安全算法 Security algorithm  仅演示! / demo only!
 *   3. 应用数据 Application data    DID 表 / the DID tables
 *   4. 用户服务 User services       0x11 ECUReset、0x31 RoutineControl
 *   5. 响应完成钩子 OnResponseSent  复位 / 跳转 bootloader
 *   6. 演示驱动 Demo driver         逐条发送请求并打印结果 / sends scripted requests
 *
 * 构建 / Build:
 *   cmake -S . -B build && cmake --build build && ./build/server_example
 */
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "MicroUdsServer.h"

/* 会话位掩码的缩写 / short names for session masks */
#define S_DEF MICROUDS_SET_SESSION(MICROUDS_SESSION_DEFAULT)
#define S_PRG MICROUDS_SET_SESSION(MICROUDS_SESSION_PROGRAM)
#define S_EXT MICROUDS_SET_SESSION(MICROUDS_SESSION_EXTEND)

static void app_init(void);

/* ======================================================================
 * 1. 移植层 / Port layer
 *    真实工程里: RespTransmit 把响应交给 ISO-TP，ISO-TP 发完后调用 HwRespTxDone；
 *    ISO-TP 收到完整请求后调用 RxIndication；定时器里调 TickHandler；主循环里调 TimerHandler。
 *    On a real target: RespTransmit hands the PDU to ISO-TP, ISO-TP calls HwRespTxDone when it is sent;
 *    ISO-TP calls RxIndication for every complete request; a timer ISR calls TickHandler; the main loop calls TimerHandler.
 * ====================================================================== */

static bool    g_tx_inflight;      /* 已交给"ISO-TP"，还没发完 / handed to the fake ISO-TP, not yet "sent" */
static uint8_t g_last_tx[64];      /* 最近一条响应，演示里用来取种子 / last response (used to read the seed) */
static uint32_t g_last_tx_len;
static int     g_resp_cnt;

static void print_hex(const uint8_t *p, size_t n)
{
    for (size_t i = 0; i < n; i++)
    {
        printf("%02X%s", p[i], (i + 1 < n) ? " " : "");
    }
}

/* 内核要发一条响应。必须非阻塞。返回 OK = 已接受。 */
MicroUds_Status_t MicroUdsServer_RespTransmit(const uint8_t *pdu, uint32_t len)
{
    printf("< ");
    print_hex(pdu, len);
    printf("\n");

    memcpy(g_last_tx, pdu, len);
    g_last_tx_len = len;
    g_resp_cnt++;
    g_tx_inflight = true; /* 真实 ISO-TP: 开始分帧发送 / real ISO-TP: start segmented transmission */
    return MICROUDS_OK;
}

/* ======================================================================
 * 2. 安全算法 / Security algorithm  —— DEMO ONLY
 *    真实产品: 种子必须来自硬件 TRNG；密钥算法要用 CMAC/AES 等，不要用 XOR。
 *    Real products: seeds must come from a hardware TRNG and the key algorithm must be a proper MAC (e.g. AES-CMAC), not XOR.
 * ====================================================================== */

static const uint8_t k_secret[MICROUDS_KEY_SIZE] = {0x5A, 0xC3, 0x19, 0xE7};

void MicroUdsServer_SecurityReqSeed(uint8_t level, uint8_t *out, uint32_t *len)
{
    static uint32_t lcg = 0x1234ABCDu;

    lcg = lcg * 1664525u + 1013904223u + level;
    for (uint32_t i = 0; i < MICROUDS_SEED_SIZE; i++)
    {
        out[i] = (uint8_t)(lcg >> (8u * i));
    }
    *len = MICROUDS_SEED_SIZE;
}

static void calc_key(uint8_t level, const uint8_t *seed, uint32_t seed_len, uint8_t *key)
{
    for (uint32_t i = 0; i < seed_len; i++)
    {
        key[i] = (uint8_t)(seed[i] ^ k_secret[i % MICROUDS_KEY_SIZE] ^ level);
    }
}

void MicroUdsServer_SecurityGetKey(uint8_t level, const uint8_t *seed, uint32_t seed_len, uint8_t *key, uint32_t *key_len)
{
    if (*key_len < seed_len)
    {
        *key_len = 0; /* 容量不够: 报告失败 / buffer too small: report failure */
        return;
    }
    calc_key(level, seed, seed_len, key);
    *key_len = seed_len;
}

/* ======================================================================
 * 3. 应用数据 / Application data (DID tables)
 *    读表和写表分开: 不在写表里的 DID 就是只读。session=0 表示"所有会话", permis=0 表示"无需解锁"。
 *    Read and write tables are separate: a DID that is not in the write table is read-only.
 *    session = 0 means "all sessions", permis = 0 means "no unlock required".
 * ====================================================================== */

static uint8_t g_vin[17] = {'M', 'I', 'C', 'R', 'O', 'U', 'D', 'S', '0', '0', '0', '0', '0', '0', '0', '0', '1'};
static uint8_t g_serial[4] = {0x00, 0x00, 0x12, 0x34};
static uint8_t g_cfg[2] = {0x12, 0x34};      /* 0x0100 读写配置 / read-write config */
static uint8_t g_temp[2];                    /* 0x0200 实时温度 / live temperature */
static uint8_t g_secret_data[2] = {0xAA, 0xBB}; /* 0x0300 需要解锁才能读 / needs unlock to read */
static uint8_t g_calib[2] = {0x00, 0x32};    /* 0x0400 标定值 0..100 / calibration 0..100 */
static uint8_t g_flash_param[4];             /* 0x0700 写入很慢(模拟 NVM) / slow write (simulated NVM) */

/* 读回调: 内核在拷贝数据之前调用，用来把最新值刷新到 ctx->data */
static void did_temp_refresh(MicroUdsServerDid_Ctx_t *ctx)
{
    static uint16_t sensor = 250; /* 模拟传感器 25.0 ℃ / simulated sensor */

    sensor++;
    ctx->data[0] = (uint8_t)(sensor >> 8);
    ctx->data[1] = (uint8_t)(sensor & 0xFFu);
}

/* 写回调: 内核校验(DID/会话/安全/长度)通过后调用。在这里做"数据层校验"。返回 nrc=0 表示接受，内核随后把数据拷进 ctx->data */
static void did_calib_write(MicroUdsServerDid_Ctx_t *ctx)
{
    uint16_t v = (uint16_t)(((uint16_t)ctx->in_data[0] << 8) | ctx->in_data[1]);

    if (v > 100u)
    {
        ctx->nrc = MICROUDS_NRC_ROOR; /* 超范围: 拒绝，g_calib 保持不变 / out of range: rejected, g_calib is untouched */
    }
}

/* 慢速写: 前 3 次调用都说"还没写完"(pending)，内核会先回 0x78，之后每个周期再调用一次 */
static void did_flash_write(MicroUdsServerDid_Ctx_t *ctx)
{
    if (ctx->call_cnt < 3u)
    {
        ctx->pending = true; /* 模拟 NVM 编程还没结束 / simulated NVM program still busy */
    }
}

static const MicroUdsServerDidItem_Config_t k_read_dids[] = {
    {.Identifier = 0xF190, .data = g_vin, .data_len = sizeof(g_vin)},         /* VIN: 所有会话可读 */
    {.Identifier = 0xF18C, .data = g_serial, .data_len = sizeof(g_serial)},   /* ECU 序列号 */
    {.Identifier = 0x0100, .data = g_cfg, .data_len = sizeof(g_cfg), .session = S_EXT | S_PRG},
    {.Identifier = 0x0200, .data = g_temp, .data_len = sizeof(g_temp), .cb = did_temp_refresh},
    {.Identifier = 0x0300, .data = g_secret_data, .data_len = sizeof(g_secret_data), .session = S_EXT, .permis = MICROUDS_PERMIS_ATLEAST(1)},
    {.Identifier = 0x0400, .data = g_calib, .data_len = sizeof(g_calib), .session = S_EXT},
    {.Identifier = 0x0700, .data = g_flash_param, .data_len = sizeof(g_flash_param), .session = S_EXT},
};

static const MicroUdsServerDidItem_Config_t k_write_dids[] = {
    {.Identifier = 0x0100, .data = g_cfg, .data_len = sizeof(g_cfg), .session = S_EXT, .permis = MICROUDS_PERMIS_ATLEAST(1)},
    {.Identifier = 0x0400, .data = g_calib, .data_len = sizeof(g_calib), .session = S_EXT, .cb = did_calib_write},
    {.Identifier = 0x0700, .data = g_flash_param, .data_len = sizeof(g_flash_param), .session = S_EXT, .cb = did_flash_write},
};

/* 0x2C 02 (按地址定义动态 DID) 的白名单。默认拒绝一切；只放行你明确列出的区间。
 * Whitelist for 0x2C 02 (dynamic DID by address). Everything is denied by default; list only the ranges you really want exposed. */
bool MicroUdsServer_Did_AddrReadable(uint32_t addr, uint32_t len)
{
    const uint32_t ram_start = 0x20000000u; /* 按你的 MCU 修改 / adjust for your MCU */
    const uint32_t ram_size = 0x100u;

    return (addr >= ram_start) && (len <= ram_size) && ((addr - ram_start) <= (ram_size - len));
}

/* ======================================================================
 * 4. 用户服务 / User services
 * ====================================================================== */

/* ---- 0x11 ECUReset: 回调只负责校验并回正响应，真正的复位在 OnResponseSent 里(响应发完之后) ---- */
static void svc_ecu_reset(MicroUdsServer_ServiceCtx_t *ctx)
{
    if (ctx->sub_func == 0x01u || ctx->sub_func == 0x03u) /* 01 hardReset, 03 softReset */
    {
        MicroUdsServer_CtxPositive(ctx, 0u); /* 响应 = 51 <sub>，子功能由内核回显 */
        return;
    }
    MicroUdsServer_CtxNegative(ctx, MICROUDS_NRC_SFNS);
}

/* ---- 0x31 RoutineControl: 每个子功能一个回调(start/stop/results)，RID 在 ctx->req[0..1] ---- */
static bool g_erase_done;

static void rc_start(MicroUdsServer_ServiceCtx_t *ctx)
{
    uint16_t rid = (uint16_t)(((uint16_t)ctx->req[0] << 8) | ctx->req[1]);

    switch (rid)
    {
    case 0xFF00: /* 擦除存储器: 耗时，用 PENDING 表达 / erase memory: slow, expressed with PENDING */
        if (ctx->call_cnt < 5u)
        {
            MicroUdsServer_CtxPending(ctx); /* 内核立刻回 0x78，之后每个周期再调用本函数 */
            return;
        }
        g_erase_done = true;
        break;

    case 0x0203: /* 自检: 立即完成 / self test: completes at once */
        break;

    default:
        MicroUdsServer_CtxNegative(ctx, MICROUDS_NRC_ROOR);
        return;
    }

    ctx->payload[0] = ctx->req[0]; /* 响应 = 71 01 <RID> <状态> */
    ctx->payload[1] = ctx->req[1];
    ctx->payload[2] = 0x00; /* 0 = 成功 / success */
    MicroUdsServer_CtxPositive(ctx, 3u);
}

static void rc_stop(MicroUdsServer_ServiceCtx_t *ctx)
{
    ctx->payload[0] = ctx->req[0];
    ctx->payload[1] = ctx->req[1];
    MicroUdsServer_CtxPositive(ctx, 2u);
}

static void rc_results(MicroUdsServer_ServiceCtx_t *ctx)
{
    ctx->payload[0] = ctx->req[0];
    ctx->payload[1] = ctx->req[1];
    ctx->payload[2] = g_erase_done ? 0x00 : 0x01; /* 0 = 完成, 1 = 未完成 */
    MicroUdsServer_CtxPositive(ctx, 3u);
}

static const MicroUdsServer_ServiceTable_t k_services[] = {
    {.sid = 0x11, .flags = MICROUDS_SERV_FLAG_HAS_SUBFUNC | MICROUDS_SERV_FLAG_SUPPRESS, .permis = 0,
     .session_mask = S_EXT | S_PRG, .min_len = 2, .max_len = 2, .callback = svc_ecu_reset},
    /* 0x31: 没有服务级回调，全部由子功能表里的回调处理 */
    {.sid = 0x31, .flags = MICROUDS_SERV_FLAG_HAS_SUBFUNC | MICROUDS_SERV_FLAG_SUPPRESS, .permis = 0,
     .session_mask = S_EXT | S_PRG, .min_len = 4, .max_len = 0, .callback = NULL},
};

static const MicroUdsServer_SubFuncTable_t k_rc_subs[] = {
    {.id = 0x01, .callback = rc_start, .permis = MICROUDS_PERMIS_ATLEAST(1)}, /* 启动例程需要先解锁等级1 */
    {.id = 0x02, .callback = rc_stop},
    {.id = 0x03, .callback = rc_results},
};

/* ======================================================================
 * 5. 响应完成钩子 / OnResponseSent
 *    "必须在响应发出之后才能做的事"都放这里。所有服务(内置的和用户的)共用这一个钩子。
 *    负响应也会触发，所以先判断 resp_type。
 * ====================================================================== */

static bool g_reset_requested;

void MicroUdsServer_OnResponseSent(MicroUdsServer_ServiceCtx_t *ctx)
{
    if (ctx->resp_type != MICROUDS_RESPTYPE_POSITIVE)
    {
        return; /* 被拒绝的请求不能触发复位/跳转 / a rejected request must never reset or jump */
    }

    if (ctx->sid == 0x11 && ctx->sub_func == 0x01u)
    {
        g_reset_requested = true; /* 真实 MCU: NVIC_SystemReset(); */
    }

    if (ctx->sid == 0x10 && ctx->sub_func == 0x02u)
    {
        printf("  [hook] 10 02 answered, session=%u -> jump to bootloader here\n", ctx->session);
        /* 真实 MCU: 设置"进入刷写"标志后复位，或直接跳转 / real MCU: set a "stay in bootloader" flag and reset, or jump directly */
    }
}

/* ======================================================================
 * 6. 演示驱动 / Demo driver
 * ====================================================================== */

/* 初始化顺序: Init -> 注册服务 -> 注册 DID 表。必须在 Init 之后注册。 */
static void app_init(void)
{
    if (MicroUdsServer_Init() != MICROUDS_OK ||
        MicroUdsServer_ServiceRegisterServ(k_services, MICROUDS_SIZEOF(k_services)) != MICROUDS_OK ||
        MicroUdsServer_ServiceRegisterSubFunc(0x31, k_rc_subs, MICROUDS_SIZEOF(k_rc_subs)) != MICROUDS_OK ||
        MicroUdsServer_Did_RegisterReadItem(k_read_dids, MICROUDS_SIZEOF(k_read_dids)) != MICROUDS_OK ||
        MicroUdsServer_Did_RegisterWriteItem(k_write_dids, MICROUDS_SIZEOF(k_write_dids)) != MICROUDS_OK)
    {
        printf("init failed\n");
        for (;;) {}
    }
}

/* 推进 1 ms: 真实系统里 TickHandler 在定时中断，TimerHandler 在主循环 */
static void step_1ms(void)
{
    MicroUdsServer_TickHandler();
    MicroUdsServer_TimerHandler();

    if (g_tx_inflight) /* 模拟 ISO-TP: 响应发完了 */
    {
        g_tx_inflight = false;
        MicroUdsServer_HwRespTxDone();
    }

    if (g_reset_requested) /* 模拟"复位": 所有 RAM 状态丢失，重新启动 */
    {
        g_reset_requested = false;
        printf("  [hook] 11 01 answered -> ECU hard reset (re-initialising)\n");
        app_init();
    }
}

static void run_request(const char *title, MicroUds_AddrType_t addr, const uint8_t *req, size_t len)
{
    printf("\n# %s\n> ", title);
    print_hex(req, len);
    printf("\n");

    g_resp_cnt = 0;

    MicroUds_Status_t st = MicroUdsServer_RxIndication(req, len, addr); /* 真实系统: ISO-TP 收到完整请求时调用 */
    if (st != MICROUDS_OK)
    {
        printf("  RxIndication returned %d\n", (int)st);
    }

    for (int i = 0; i < 40; i++)
    {
        step_1ms();
    }

    if (g_resp_cnt == 0)
    {
        printf("  (no response)\n");
    }
}

#define REQ(title, ...) do { const uint8_t r_[] = {__VA_ARGS__}; run_request(title, MICROUDS_ADDR_PHY, r_, sizeof r_); } while (0)
#define REQ_FUNC(title, ...) do { const uint8_t r_[] = {__VA_ARGS__}; run_request(title, MICROUDS_ADDR_FUNC, r_, sizeof r_); } while (0)

int main(void)
{
    app_init();
    printf("MicroUds %s server example\n", MICROUDS_VERSION);

    printf("\n========== Sessions ==========");
    REQ("enter extended session", 0x10, 0x03);
    REQ("TesterPresent, positive response suppressed (3E 80)", 0x3E, 0x80);
    REQ("session 5 does not exist", 0x10, 0x05);

    printf("\n========== Read DIDs (0x22) ==========");
    REQ("read VIN", 0x22, 0xF1, 0x90);
    REQ("read serial + live temperature (two DIDs in one request)", 0x22, 0xF1, 0x8C, 0x02, 0x00);
    REQ("read an unknown DID", 0x22, 0x99, 0x99);
    REQ("read a DID that needs security (not unlocked yet)", 0x22, 0x03, 0x00);

    printf("\n========== Security access (0x27) ==========");
    REQ("request seed, level 1", 0x27, 0x01);
    {
        uint8_t key[MICROUDS_KEY_SIZE];
        uint8_t send_key[2 + MICROUDS_KEY_SIZE] = {0x27, 0x02};

        calc_key(1, &g_last_tx[2], MICROUDS_SEED_SIZE, key); /* 诊断仪: 用种子算出密钥 / the tester computes the key from the seed */
        memcpy(&send_key[2], key, sizeof key);
        run_request("send key", MICROUDS_ADDR_PHY, send_key, sizeof send_key);
    }
    REQ("read the secured DID again", 0x22, 0x03, 0x00);

    printf("\n========== Write DIDs (0x2E) ==========");
    REQ("write config 0x0100", 0x2E, 0x01, 0x00, 0xBE, 0xEF);
    REQ("read it back", 0x22, 0x01, 0x00);
    REQ("write config with the wrong length", 0x2E, 0x01, 0x00, 0xBE);
    REQ("write read-only VIN", 0x2E, 0xF1, 0x90, 0x01, 0x02, 0x03);
    REQ("write calibration = 200 (out of range, rejected by the callback)", 0x2E, 0x04, 0x00, 0x00, 0xC8);
    REQ("write calibration = 60", 0x2E, 0x04, 0x00, 0x00, 0x3C);
    REQ("slow write (answers 0x78 first, then the final response)", 0x2E, 0x07, 0x00, 0xCA, 0xFE, 0xBA, 0xBE);

    printf("\n========== Dynamic DID (0x2C) ==========");
    REQ("define F300 = VIN[1..3] + config[1..2]", 0x2C, 0x01, 0xF3, 0x00, 0xF1, 0x90, 0x01, 0x03, 0x01, 0x00, 0x01, 0x02);
    REQ("read F300", 0x22, 0xF3, 0x00);
    REQ("clear F300", 0x2C, 0x03, 0xF3, 0x00);
    REQ("read F300 again", 0x22, 0xF3, 0x00);

    printf("\n========== RoutineControl (0x31, user service) ==========");
    REQ("start self-test routine 0x0203", 0x31, 0x01, 0x02, 0x03);
    REQ("start erase routine 0xFF00 (slow)", 0x31, 0x01, 0xFF, 0x00);
    REQ("request erase results", 0x31, 0x03, 0xFF, 0x00);
    REQ("unknown routine", 0x31, 0x01, 0x12, 0x34);

    printf("\n========== Unknown service / functional addressing ==========");
    REQ("unknown service", 0x99, 0x00);
    REQ_FUNC("read VIN with functional addressing (0x22 is physical-only)", 0x22, 0xF1, 0x90);
    REQ_FUNC("TesterPresent with functional addressing", 0x3E, 0x00);

    printf("\n========== Actions after the response (OnResponseSent) ==========");
    REQ("ECUReset hard (the reset happens AFTER the response)", 0x11, 0x01);
    REQ("after the reset the ECU is back in the default session: 0x11 is not available", 0x11, 0x01);
    REQ("enter extended session again", 0x10, 0x03);
    REQ("11 05: rejected by the service, must NOT reset", 0x11, 0x05);
    REQ("enter programming session (hook jumps to the bootloader)", 0x10, 0x02);

    printf("\n");
    return 0;
}