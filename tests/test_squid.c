/* tests/test_squid.c – loopback test for libsquid.
 *
 * Two squid instances (A and B) connected back-to-back through shared
 * ring buffers.  Each call to snet_burst() on one side may produce
 * bytes that the other side can read.
 *
 * We call burst in a loop to drive the handshake, then push data
 * through squid_send/squid_recv and verify it arrives correctly.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "squid/snet.h"
#include "squid/socket.h"
#include "squid/internal.h"   /* access g_snet for swapping contexts */

/* ================================================================== */
/*  Simulated wire: two ring buffers (A→B and B→A)                    */
/* ================================================================== */
#define RING_SIZE 4096

typedef struct {
    uint8_t buf[RING_SIZE];
    uint16_t head, tail;      /* head = write, tail = read */
} ring_t;

static ring_t wire_a2b;       /* bytes A sends, B reads */
static ring_t wire_b2a;       /* bytes B sends, A reads */

static void ring_reset(ring_t *r) { r->head = r->tail = 0; }

static int ring_put(ring_t *r, uint8_t c)
{
    uint16_t next = (uint16_t)((r->head + 1) % RING_SIZE);
    if (next == r->tail) return -1;     /* full */
    r->buf[r->head] = c;
    r->head = next;
    return 0;
}

static int ring_get(ring_t *r)
{
    if (r->head == r->tail) return -1;  /* empty */
    uint8_t c = r->buf[r->tail];
    r->tail = (uint16_t)((r->tail + 1) % RING_SIZE);
    return c;
}

/* ================================================================== */
/*  Platform hooks — one set for each side                            */
/* ================================================================== */
static uint8_t fake_tick = 0;

/* Side A: sends into wire_a2b, receives from wire_b2a */
static int a_send(uint8_t c)  { return ring_put(&wire_a2b, c); }
static int a_recv(void)       { return ring_get(&wire_b2a); }
static uint8_t a_tick(void)   { return fake_tick; }
static void* a_malloc(uint16_t n) { return malloc(n); }
static void  a_free(void *p)      { free(p); }

/* Side B: sends into wire_b2a, receives from wire_a2b */
static int b_send(uint8_t c)  { return ring_put(&wire_b2a, c); }
static int b_recv(void)       { return ring_get(&wire_a2b); }
static uint8_t b_tick(void)   { return fake_tick; }
static void* b_malloc(uint16_t n) { return malloc(n); }
static void  b_free(void *p)      { free(p); }

static const squid_platform_t plat_a = {
    .send_char = a_send, .recv_char = a_recv, .get_tick = a_tick,
    .malloc = a_malloc, .free = a_free
};
static const squid_platform_t plat_b = {
    .send_char = b_send, .recv_char = b_recv, .get_tick = b_tick,
    .malloc = b_malloc, .free = b_free
};

/* ================================================================== */
/*  Context swapping — save/restore g_snet for each side              */
/* ================================================================== */
static snet_ctx_t ctx_a, ctx_b;

static void load_a(void) { g_snet = ctx_a; }
static void load_b(void) { g_snet = ctx_b; }
static void save_a(void) { ctx_a = g_snet; }
static void save_b(void) { ctx_b = g_snet; }

/* ================================================================== */
/*  Helper: pump both sides for N ticks                               */
/* ================================================================== */
static void pump(int ticks)
{
    for (int t = 0; t < ticks; t++) {
        fake_tick++;

        load_a();
        snet_burst();
        save_a();

        load_b();
        snet_burst();
        save_b();
    }
}

/* ================================================================== */
/*  Test infrastructure                                               */
/* ================================================================== */
static int tests_run = 0, tests_passed = 0;

#define TEST(name) static int name(void)
#define ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("  FAIL: %s (line %d): %s\n", __func__, __LINE__, msg); \
        return 0; \
    } \
} while(0)
#define RUN(fn) do { \
    tests_run++; \
    printf("  %-40s ", #fn); \
    if (fn()) { tests_passed++; printf("OK\n"); } \
    else printf("\n"); \
} while(0)

/* ================================================================== */
/*  Setup: init both sides fresh                                      */
/* ================================================================== */
static void setup(void)
{
    ring_reset(&wire_a2b);
    ring_reset(&wire_b2a);
    fake_tick = 0;
    memset(&ctx_a, 0, sizeof(ctx_a));
    memset(&ctx_b, 0, sizeof(ctx_b));
    memset(&g_snet, 0, sizeof(g_snet));

    squid_timing_t tm = { .timeout_ticks = 3, .ack_delay_ticks = 1,
                          .ping_ticks = 0, .max_retries = 5 };

    snet_init(&plat_a, &tm);
    save_a();

    snet_init(&plat_b, &tm);
    save_b();
}

/* ================================================================== */
/*  Tests: snet layer                                                 */
/* ================================================================== */

TEST(test_null_platform_fails)
{
    memset(&g_snet, 0, sizeof(g_snet));
    snet_init(NULL, NULL);
    ASSERT(g_snet.eng == SNET_ENG_DISCONNECTED,
           "null platform should leave engine disconnected");
    return 1;
}

TEST(test_init_state)
{
    setup();
    load_a();
    ASSERT(g_snet.eng == SNET_ENG_STARTUP, "A should start in STARTUP");
    ASSERT(!snet_link_is_up(), "A link should be down");
    save_a();

    load_b();
    ASSERT(g_snet.eng == SNET_ENG_STARTUP, "B should start in STARTUP");
    ASSERT(!snet_link_is_up(), "B link should be down");
    save_b();
    return 1;
}

TEST(test_link_down_after_init)
{
    setup();
    load_a();
    ASSERT(!snet_link_is_up(), "link should be down before handshake");
    save_a();
    return 1;
}

TEST(test_handshake)
{
    setup();
    pump(20);

    load_a();
    ASSERT(snet_link_is_up(), "A should be connected after handshake");
    save_a();

    load_b();
    ASSERT(snet_link_is_up(), "B should be connected after handshake");
    save_b();
    return 1;
}

/* ================================================================== */
/*  Tests: socket layer                                               */
/* ================================================================== */

TEST(test_open_close_socket)
{
    setup();
    pump(20);

    load_a();
    int ch = squid_open();
    ASSERT(ch >= 1 && ch <= 15, "channel id should be 1..15");
    squid_close(ch);
    save_a();

    return 1;
}

TEST(test_open_max_sockets)
{
    setup();
    pump(20);

    load_a();
    int ids[15];
    for (int i = 0; i < 15; i++) {
        ids[i] = squid_open();
        ASSERT(ids[i] >= 1, "should be able to open 15 sockets");
    }
    int overflow = squid_open();
    ASSERT(overflow == -1, "16th socket should fail");

    for (int i = 0; i < 15; i++) squid_close(ids[i]);
    save_a();
    return 1;
}

TEST(test_send_recv_single)
{
    setup();
    pump(20);

    /* open socket 1 on both sides */
    load_a();
    int sa = squid_open();
    ASSERT(sa == 1, "A socket should be 1");
    save_a();

    load_b();
    int sb = squid_open();
    ASSERT(sb == 1, "B socket should be 1");
    save_b();

    /* A sends 5 bytes */
    load_a();
    uint8_t msg[] = { 'H', 'E', 'L', 'L', 'O' };
    int sent = squid_send(sa, msg, 5);
    ASSERT(sent == 5, "send should accept 5 bytes");
    save_a();

    pump(30);

    /* B receives */
    load_b();
    uint8_t buf[16];
    int got = squid_recv(sb, buf, sizeof(buf));
    ASSERT(got == 5, "recv should return 5");
    ASSERT(memcmp(buf, msg, 5) == 0, "received data should match");
    save_b();

    return 1;
}

TEST(test_bidirectional)
{
    setup();
    pump(20);

    load_a();
    int sa = squid_open();
    save_a();

    load_b();
    int sb = squid_open();
    save_b();

    /* A sends to B */
    load_a();
    uint8_t msg_ab[] = { 'A', 'B' };
    squid_send(sa, msg_ab, 2);
    save_a();

    /* B sends to A */
    load_b();
    uint8_t msg_ba[] = { 'B', 'A' };
    squid_send(sb, msg_ba, 2);
    save_b();

    pump(30);

    /* B receives from A */
    load_b();
    uint8_t buf[16];
    int got = squid_recv(sb, buf, sizeof(buf));
    ASSERT(got == 2, "B should receive 2 bytes from A");
    ASSERT(buf[0] == 'A' && buf[1] == 'B', "B data should be AB");
    save_b();

    /* A receives from B */
    load_a();
    got = squid_recv(sa, buf, sizeof(buf));
    ASSERT(got == 2, "A should receive 2 bytes from B");
    ASSERT(buf[0] == 'B' && buf[1] == 'A', "A data should be BA");
    save_a();

    return 1;
}

TEST(test_large_transfer)
{
    setup();
    pump(20);

    load_a();
    int sa = squid_open();
    save_a();

    load_b();
    int sb = squid_open();
    save_b();

    /* send 100 bytes (needs multiple 15-byte frames) */
    uint8_t data[100];
    for (int i = 0; i < 100; i++) data[i] = (uint8_t)i;

    load_a();
    int sent = squid_send(sa, data, 100);
    ASSERT(sent == 100, "should queue 100 bytes");
    save_a();

    pump(300);

    load_b();
    uint8_t recv_buf[100];
    memset(recv_buf, 0xFF, sizeof(recv_buf));
    int got = squid_recv(sb, recv_buf, sizeof(recv_buf));
    ASSERT(got == 100, "should receive 100 bytes");
    ASSERT(memcmp(recv_buf, data, 100) == 0, "large transfer data should match");
    save_b();

    return 1;
}

TEST(test_two_sockets_isolated)
{
    setup();
    pump(20);

    /* open sockets 1 and 2 on both sides */
    load_a();
    int sa1 = squid_open();
    int sa2 = squid_open();
    ASSERT(sa1 == 1, "A socket 1");
    ASSERT(sa2 == 2, "A socket 2");
    save_a();

    load_b();
    int sb1 = squid_open();
    int sb2 = squid_open();
    ASSERT(sb1 == 1, "B socket 1");
    ASSERT(sb2 == 2, "B socket 2");
    save_b();

    /* send different data on each socket */
    load_a();
    uint8_t msg1[] = { 0x11, 0x22 };
    uint8_t msg2[] = { 0xAA, 0xBB, 0xCC };
    squid_send(sa1, msg1, 2);
    squid_send(sa2, msg2, 3);
    save_a();

    pump(60);

    /* B receives from socket 1 — should get msg1 only */
    load_b();
    uint8_t buf[16];
    int got1 = squid_recv(sb1, buf, sizeof(buf));
    ASSERT(got1 == 2, "socket 1 should receive 2 bytes");
    ASSERT(buf[0] == 0x11 && buf[1] == 0x22, "socket 1 data should match");

    /* B receives from socket 2 — should get msg2 only */
    int got2 = squid_recv(sb2, buf, sizeof(buf));
    ASSERT(got2 == 3, "socket 2 should receive 3 bytes");
    ASSERT(buf[0] == 0xAA && buf[1] == 0xBB && buf[2] == 0xCC,
           "socket 2 data should match");
    save_b();

    return 1;
}

/* ================================================================== */
/*  Main                                                              */
/* ================================================================== */
int main(void)
{
    printf("libsquid test suite\n");
    printf("===================\n");

    /* snet layer */
    RUN(test_null_platform_fails);
    RUN(test_init_state);
    RUN(test_link_down_after_init);
    RUN(test_handshake);

    /* socket layer */
    RUN(test_open_close_socket);
    RUN(test_open_max_sockets);
    RUN(test_send_recv_single);
    RUN(test_bidirectional);
    RUN(test_large_transfer);
    RUN(test_two_sockets_isolated);

    printf("===================\n");
    printf("%d/%d tests passed\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? 0 : 1;
}
