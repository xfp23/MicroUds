/**
 * @file test_MicroUds.c
 * @brief MicroUds 主机侧(PC)自测，覆盖 code review 中发现的问题
 *        编译: gcc -std=c11 -Wall -Wextra -I. MicroUds.c test/test_MicroUds.c -o test_uds && ./test_uds
 */
#include <stdio.h>
#include "MicroUds.h"

static uint8_t g_tx[128];
static uint32_t g_tx_len;
static int g_tx_cnt;
static MicroUds_Status_t g_tx_ret = MICROUDS_OK;
static int g_fail;

MicroUds_Status_t MicroUds_Transmit(const uint8_t *pdu, uint32_t len)
{
    if (g_tx_ret == MICROUDS_OK)
    {
        memcpy(g_tx, pdu, len);
        g_tx_len = len;
        g_tx_cnt++;
    }
    return g_tx_ret;
}

void MicroUds_SecurityReqSeed(uint8_t level, uint8_t *out, uint32_t *len)
{
    for (uint32_t i = 0; i < 4; i++) out[i] = (uint8_t)(0x10u * level + i + 1u);
    *len = 4;
}

void MicroUds_SecurityGetKey(uint8_t level, const uint8_t *seed, uint32_t seed_len, uint8_t *key, uint32_t *key_len)
{
    for (uint32_t i = 0; i < seed_len; i++) key[i] = (uint8_t)(seed[i] ^ 0xA5u ^ level);
    *key_len = seed_len;
}

static void run(uint32_t ticks)
{
    for (uint32_t i = 0; i < ticks; i++) { MicroUds_TickHandler(); MicroUds_TimerHandler(); }
}

/* 发请求，跑几个周期，期望收到 exp。exp_len==0 表示期望"无响应" */
static void req(const char *name, MicroUds_AddrType_t a, const uint8_t *r, size_t rl, const uint8_t *exp, uint32_t el)
{
    g_tx_cnt = 0;
    MicroUds_Status_t st = MicroUds_RxIndication(r, rl, a);
    run(3);
    int ok = (st == MICROUDS_OK);
    if (el == 0) ok = ok && (g_tx_cnt == 0);
    else ok = ok && (g_tx_cnt == 1) && (g_tx_len == el) && (memcmp(g_tx, exp, el) == 0);
    printf("[%s] %s", ok ? "PASS" : "FAIL", name);
    if (!ok)
    {
        g_fail++;
        printf("  got(%d):", g_tx_cnt);
        for (uint32_t i = 0; i < g_tx_len && g_tx_cnt; i++) printf(" %02X", g_tx[i]);
    }
    printf("\n");
}
#define PHY MICROUDS_ADDR_PHY
#define FUNC MICROUDS_ADDR_FUNC
#define REQ(n, a, ...) do { const uint8_t r_[] = {__VA_ARGS__}; req(n, a, r_, sizeof(r_), NULL, 0); } while (0)
#define EXPECT(n, a, rq, rs) do { const uint8_t r_[] = rq; const uint8_t e_[] = rs; req(n, a, r_, sizeof(r_), e_, sizeof(e_)); } while (0)
#define Q(...) {__VA_ARGS__}

/* -------- 测试用户服务 -------- */
static uint8_t g_did_f190[4] = {'V', 'I', 'N', '1'};

// 0x22 无子功能，服务级回调，单个DID
static void svc_read_did(MicroUds_ServiceCtx_t *ctx)
{
    if (ctx->req_len != 2) { MicroUds_CtxNegative(ctx, MICROUDS_NRC_IMLOI); return; }
    if (ctx->req[0] == 0xF1 && ctx->req[1] == 0x90)
    {
        ctx->payload[0] = 0xF1; ctx->payload[1] = 0x90;
        memcpy(&ctx->payload[2], g_did_f190, 4);
        MicroUds_CtxPositive(ctx, 6);
        return;
    }
    MicroUds_CtxNegative(ctx, MICROUDS_NRC_ROOR);
}

// 0x2E 需要安全等级1
static void svc_write_did(MicroUds_ServiceCtx_t *ctx)
{
    ctx->payload[0] = ctx->req[0]; ctx->payload[1] = ctx->req[1];
    MicroUds_CtxPositive(ctx, 2);
}

// 0x31 子功能 01 启动(异步) / 02 停止
static int g_rtn_polls;
static void rtn_start(MicroUds_ServiceCtx_t *ctx)
{
    if (ctx->call_cnt == 0) g_rtn_polls = 0;
    if (++g_rtn_polls < 5000) { MicroUds_CtxPending(ctx); return; }  /* 约5000个tick后才完成 */
    ctx->payload[0] = ctx->req[0]; ctx->payload[1] = ctx->req[1]; ctx->payload[2] = 0x00;
    MicroUds_CtxPositive(ctx, 3);
}
static void rtn_stop(MicroUds_ServiceCtx_t *ctx)
{
    ctx->payload[0] = ctx->req[0]; ctx->payload[1] = ctx->req[1];
    MicroUds_CtxPositive(ctx, 2);
}
// 0x85 子功能 01，只在编程会话可用(用于测 0x7E)
static void sub_dummy(MicroUds_ServiceCtx_t *ctx) { MicroUds_CtxPositive(ctx, 0); }

int main(void)
{
    MicroUds_Init();

    const MicroUds_ServiceTable_t table[] = {
        {.sid = 0x22, .flags = MICROUDS_SERV_FLAG_FUNC_ADDR, .permis = 0,
         .session_mask = MICROUDS_SET_SESSION(MICROUDS_SESSION_DEFAULT) | MICROUDS_SET_SESSION(MICROUDS_SESSION_EXTEND),
         .min_len = 3, .max_len = 3, .callback = svc_read_did},
        {.sid = 0x2E, .flags = 0, .permis = MICROUDS_PERMIS_ATLEAST(1),
         .session_mask = MICROUDS_SET_SESSION(MICROUDS_SESSION_EXTEND), .min_len = 3, .callback = svc_write_did},
        {.sid = 0x31, .flags = MICROUDS_SERV_FLAG_HAS_SUBFUNC, .permis = 0,
         .session_mask = MICROUDS_SET_SESSION(MICROUDS_SESSION_EXTEND) | MICROUDS_SET_SESSION(MICROUDS_SESSION_PROGRAM), .min_len = 4},
        {.sid = 0x85, .flags = MICROUDS_SERV_FLAG_HAS_SUBFUNC | MICROUDS_SERV_FLAG_SUPPRESS, .permis = 0,
         .session_mask = MICROUDS_SET_SESSION(MICROUDS_SESSION_DEFAULT) | MICROUDS_SET_SESSION(MICROUDS_SESSION_EXTEND) | MICROUDS_SET_SESSION(MICROUDS_SESSION_PROGRAM)},
    };
    printf("register: %d (expect 0)\n", MicroUds_ServiceRegisterServ(table, 4));
    printf("dup register: %d (expect 3=PARAM_ERR)\n", MicroUds_ServiceRegisterServ(table, 1));
    const MicroUds_SubFuncTable_t subs[] = {
        {.id = 0x01, .callback = rtn_start}, {.id = 0x02, .callback = rtn_stop}};
    printf("register sub: %d (expect 0)\n", MicroUds_ServiceRegisterSubFunc(0x31, subs, 2));
    const MicroUds_SubFuncTable_t subs85[] = {
        {.id = 0x01, .callback = sub_dummy, .session_mask = MICROUDS_SET_SESSION(MICROUDS_SESSION_PROGRAM)},
        {.id = 0x02, .callback = sub_dummy}};
    MicroUds_ServiceRegisterSubFunc(0x85, subs85, 2);
    printf("---- 基础 ----\n");

    EXPECT("22 F190 in default", PHY, Q(0x22, 0xF1, 0x90), Q(0x62, 0xF1, 0x90, 'V', 'I', 'N', '1'));
    EXPECT("22 with 1 byte -> 13", PHY, Q(0x22), Q(0x7F, 0x22, 0x13));
    EXPECT("22 unknown DID -> 31", PHY, Q(0x22, 0x12, 0x34), Q(0x7F, 0x22, 0x31));
    EXPECT("unknown SID phys -> 11", PHY, Q(0x99, 0x00), Q(0x7F, 0x99, 0x11));
    REQ("unknown SID func -> silent", FUNC, 0x99, 0x00);
    EXPECT("2E in default -> 7F (service not in session)", PHY, Q(0x2E, 0xF1, 0x90), Q(0x7F, 0x2E, 0x7F));
    EXPECT("27 in default -> 7F", PHY, Q(0x27, 0x01), Q(0x7F, 0x27, 0x7F));
    EXPECT("27 len 1 in default -> 7F (session first)", PHY, Q(0x27), Q(0x7F, 0x27, 0x7F));
    EXPECT("10 len 1 -> 13", PHY, Q(0x10), Q(0x7F, 0x10, 0x13));

    printf("---- 会话 ----\n");
    EXPECT("10 05 unsupported -> 12", PHY, Q(0x10, 0x05), Q(0x7F, 0x10, 0x12));
    EXPECT("10 03", PHY, Q(0x10, 0x03), Q(0x50, 0x03, 0x00, 0x32, 0x01, 0xF4));
    printf("session=%d (expect 3)\n", MicroUds_GetSession());
    EXPECT("3E 00", PHY, Q(0x3E, 0x00), Q(0x7E, 0x00));
    REQ("3E 80 suppress -> silent", PHY, 0x3E, 0x80);
    EXPECT("3E 01 -> 12", PHY, Q(0x3E, 0x01), Q(0x7F, 0x3E, 0x12));
    EXPECT("3E 81 bad sub with suppress bit -> negative still sent", PHY, Q(0x3E, 0x81), Q(0x7F, 0x3E, 0x12));
    EXPECT("3E functional 00 -> positive sent", FUNC, Q(0x3E, 0x00), Q(0x7E, 0x00));
    EXPECT("22 F190 in extended", PHY, Q(0x22, 0xF1, 0x90), Q(0x62, 0xF1, 0x90, 'V', 'I', 'N', '1'));
    EXPECT("85 02 (no session limit)", PHY, Q(0x85, 0x02), Q(0xC5, 0x02));
    EXPECT("85 01 only in programming -> 7E", PHY, Q(0x85, 0x01), Q(0x7F, 0x85, 0x7E));
    EXPECT("85 05 -> 12", PHY, Q(0x85, 0x05), Q(0x7F, 0x85, 0x12));
    REQ("85 82 (suppress) -> silent", PHY, 0x85, 0x82);
    REQ("22 unknown DID, functional -> 0x31 silent", FUNC, 0x22, 0x12, 0x34);
    EXPECT("22 F190, functional -> positive sent", FUNC, Q(0x22, 0xF1, 0x90), Q(0x62, 0xF1, 0x90, 'V', 'I', 'N', '1'));
    REQ("85 01 func (service has no FUNC flag) -> ignored", FUNC, 0x85, 0x02);

    printf("---- 安全访问 ----\n");
    EXPECT("2E without unlock -> 33", PHY, Q(0x2E, 0xF1, 0x90, 0x01), Q(0x7F, 0x2E, 0x33));
    EXPECT("27 02 without seed -> 24", PHY, Q(0x27, 0x02, 0, 0, 0, 0), Q(0x7F, 0x27, 0x24));
    EXPECT("27 01 seed", PHY, Q(0x27, 0x01), Q(0x67, 0x01, 0x11, 0x12, 0x13, 0x14));
    EXPECT("27 02 wrong key -> 35", PHY, Q(0x27, 0x02, 1, 2, 3, 4), Q(0x7F, 0x27, 0x35));
    EXPECT("27 02 again (seed consumed) -> 24", PHY, Q(0x27, 0x02, 1, 2, 3, 4), Q(0x7F, 0x27, 0x24));
    EXPECT("27 01 seed", PHY, Q(0x27, 0x01), Q(0x67, 0x01, 0x11, 0x12, 0x13, 0x14));
    EXPECT("27 02 short key -> 13", PHY, Q(0x27, 0x02, 1, 2), Q(0x7F, 0x27, 0x13));
    /* key = seed ^ 0xA5 ^ level(1) */
    EXPECT("27 02 right key", PHY, Q(0x27, 0x02, 0x11 ^ 0xA5 ^ 1, 0x12 ^ 0xA5 ^ 1, 0x13 ^ 0xA5 ^ 1, 0x14 ^ 0xA5 ^ 1), Q(0x67, 0x02));
    printf("sec level=%d (expect 1)\n", MicroUds_GetSecurityLevel());
    EXPECT("2E after unlock", PHY, Q(0x2E, 0xF1, 0x90, 0x01), Q(0x6E, 0xF1, 0x90));
    EXPECT("27 01 already unlocked -> zero seed", PHY, Q(0x27, 0x01), Q(0x67, 0x01, 0, 0, 0, 0));
    EXPECT("27 03 level2 seed", PHY, Q(0x27, 0x03), Q(0x67, 0x03, 0x21, 0x22, 0x23, 0x24));
    EXPECT("27 04 key for level2 right", PHY, Q(0x27, 0x04, 0x21 ^ 0xA5 ^ 2, 0x22 ^ 0xA5 ^ 2, 0x23 ^ 0xA5 ^ 2, 0x24 ^ 0xA5 ^ 2), Q(0x67, 0x04));
    EXPECT("10 03 again (same session) keeps unlock? check lvl", PHY, Q(0x10, 0x03), Q(0x50, 0x03, 0x00, 0x32, 0x01, 0xF4));
    printf("sec level after same-session 10 03 = %d (same session: not relocked)\n", MicroUds_GetSecurityLevel());
    EXPECT("10 02 programming", PHY, Q(0x10, 0x02), Q(0x50, 0x02, 0x00, 0x32, 0x01, 0xF4));
    printf("sec level after session change = %d (expect 0)\n", MicroUds_GetSecurityLevel());
    EXPECT("85 01 in programming ok", PHY, Q(0x85, 0x01), Q(0xC5, 0x01));
    EXPECT("10 03 back to ext", PHY, Q(0x10, 0x03), Q(0x50, 0x03, 0x00, 0x32, 0x01, 0xF4));

    printf("---- 失败次数/延时 ----\n");
    for (int i = 0; i < 2; i++)
    {
        EXPECT("27 01", PHY, Q(0x27, 0x01), Q(0x67, 0x01, 0x11, 0x12, 0x13, 0x14));
        EXPECT("wrong key -> 35", PHY, Q(0x27, 0x02, 9, 9, 9, 9), Q(0x7F, 0x27, 0x35));
    }
    EXPECT("27 01", PHY, Q(0x27, 0x01), Q(0x67, 0x01, 0x11, 0x12, 0x13, 0x14));
    EXPECT("3rd wrong key -> 36", PHY, Q(0x27, 0x02, 9, 9, 9, 9), Q(0x7F, 0x27, 0x36));
    EXPECT("27 01 during delay -> 37", PHY, Q(0x27, 0x01), Q(0x7F, 0x27, 0x37));
    EXPECT("27 01 during delay -> 37 (still)", PHY, Q(0x27, 0x01), Q(0x7F, 0x27, 0x37));

    printf("---- 0x78 Pending ----\n");
    g_tx_cnt = 0;
    {
        const uint8_t r[] = {0x31, 0x01, 0xFF, 0x00};
        MicroUds_RxIndication(r, sizeof(r), PHY);
    }
    run(3);
    printf("[%s] first 0x78 sent: %02X %02X %02X (cnt %d)\n",
           (g_tx_cnt == 1 && g_tx[2] == 0x78) ? "PASS" : "FAIL", g_tx[0], g_tx[1], g_tx[2], g_tx_cnt);
    if (!(g_tx_cnt == 1 && g_tx[2] == 0x78)) g_fail++;
    {
        const uint8_t r[] = {0x3E, 0x00};
        MicroUds_Status_t st = MicroUds_RxIndication(r, 2, PHY);
        printf("[%s] new request while pending -> BUSY (%d)\n", st == MICROUDS_BUSY ? "PASS" : "FAIL", st);
        if (st != MICROUDS_BUSY) g_fail++;
    }
    g_tx_cnt = 0;
    run(4200); /* 超过 4000ms 再补发一次 0x78 */
    printf("[%s] resend 0x78 after ~4s (cnt %d)\n", (g_tx_cnt >= 1 && g_tx[2] == 0x78) ? "PASS" : "FAIL", g_tx_cnt);
    if (!(g_tx_cnt >= 1 && g_tx[2] == 0x78)) g_fail++;
    g_tx_cnt = 0;
    run(1500); /* 服务在 ~5000 次轮询后完成 */
    printf("[%s] final positive: %02X %02X %02X %02X\n",
           (g_tx_cnt >= 1 && g_tx[0] == 0x71 && g_tx[1] == 0x01) ? "PASS" : "FAIL", g_tx[0], g_tx[1], g_tx[2], g_tx[3]);
    if (!(g_tx_cnt >= 1 && g_tx[0] == 0x71 && g_tx[1] == 0x01)) g_fail++;
    EXPECT("31 03 sub not in table -> 12", PHY, Q(0x31, 0x03, 0xFF, 0x00), Q(0x7F, 0x31, 0x12));
    EXPECT("31 01 len too short -> 13", PHY, Q(0x31, 0x02, 0xFF), Q(0x7F, 0x31, 0x13));

    printf("---- 传输忙重试 / 请求过长 / S3 ----\n");
    g_tx_ret = MICROUDS_BUSY;
    g_tx_cnt = 0;
    { const uint8_t r[] = {0x3E, 0x00}; MicroUds_RxIndication(r, 2, PHY); }
    run(5);
    g_tx_ret = MICROUDS_OK;
    run(2);
    printf("[%s] transmit BUSY then retry: cnt=%d\n", g_tx_cnt == 1 ? "PASS" : "FAIL", g_tx_cnt);
    if (g_tx_cnt != 1) g_fail++;
    {
        uint8_t big[200] = {0x22};
        req("oversize request -> 13", PHY, big, sizeof(big), (const uint8_t[]){0x7F, 0x22, 0x13}, 3);
    }
    run(5100);
    printf("[%s] S3 timeout -> session=%d (expect 1)\n", MicroUds_GetSession() == 1 ? "PASS" : "FAIL", MicroUds_GetSession());
    if (MicroUds_GetSession() != 1) g_fail++;
    EXPECT("2E after S3 -> 7F", PHY, Q(0x2E, 0xF1, 0x90, 0x01), Q(0x7F, 0x2E, 0x7F));

    printf("\n%s (%d failed)\n", g_fail ? "SOME TESTS FAILED" : "ALL TESTS PASSED", g_fail);
    return g_fail ? 1 : 0;
}
