#include "squid.h"
#include "protocol_internal.h"
#include <string.h>

#define TIMEOUT_TICKS 10
#define MAX_RETRIES 3

static const squid_platform_t *plat = NULL;

static enum {
    STATE_STARTUP,
    STATE_WAITING_ACK,
    STATE_CONNECTED,
    STATE_DISCONNECTED
} state = STATE_STARTUP;

static uint8_t seq_sent = 0;
static uint8_t seq_recv = 0;
static uint8_t retries = 0;
static uint8_t last_tick = 0;
static uint8_t send_buffer[SQUID_DATA_SIZE];
static uint8_t recv_buffer[SQUID_DATA_SIZE];
static uint8_t last_sent[SQUID_BLOCK_SIZE];

// ---- Protocol constants ----
#define STX 0x7E
#define ETX 0xD3
#define CTRL_STX 0xE0
#define CTRL_ETX 0xCF
#define STATUS_OK 0xC3
#define STATUS_NAK 0x3C
#define TYPE_DATA 0x01
#define TYPE_PING 0x00
#define TYPE_HELLO 0x10
#define TYPE_HELLO_ACK 0x11

#define CMD_PING 0x01

static const uint8_t hello_payload[SQUID_DATA_SIZE] = {
    'H', 'E', 'L', 'L', 'O', '_', '8', 'B', 'I', 'T', '_', '_', '_', '_', '_', '_'};

static uint8_t calc_hash(const uint8_t *data)
{
    uint8_t h = 0;
    for (int i = 1; i <= 21; ++i)
        h ^= data[i];
    return h;
}

static void send_block(uint8_t seq, uint8_t ack, uint8_t status, uint8_t type, const uint8_t *payload)
{
    uint8_t blk[SQUID_BLOCK_SIZE];
    blk[0] = STX;
    blk[1] = seq;
    blk[2] = ack;
    blk[3] = status;
    blk[4] = type;
    blk[5] = 0; // flags
    memcpy(&blk[6], payload, SQUID_DATA_SIZE);
    blk[22] = calc_hash(blk);
    blk[23] = ETX;

    for (int i = 0; i < SQUID_BLOCK_SIZE; ++i)
        plat->send_char(blk[i]);
    memcpy(last_sent, blk, SQUID_BLOCK_SIZE);
    last_tick = plat->get_tick();
}

static void send_control(uint8_t seq, uint8_t ack, uint8_t cmd)
{
    uint8_t blk[5] = {CTRL_STX, seq, ack, cmd, CTRL_ETX};
    for (int i = 0; i < 5; ++i)
        plat->send_char(blk[i]);
    last_tick = plat->get_tick();
}

static bool timeout(uint8_t now, uint8_t since, uint8_t delay)
{
    return (uint8_t)(now - since) >= delay;
}

static bool recv_block(uint8_t *out_block)
{
    static uint8_t buffer[SQUID_BLOCK_SIZE];
    static int index = 0;

    while (true)
    {
        int c = plat->recv_char();
        if (c == -1)
            break;
        buffer[index++] = (uint8_t)c;

        if (index >= 2 && buffer[0] == CTRL_STX && index == 5)
        {
            // Control block
            uint8_t seq = buffer[1];
            uint8_t ack = buffer[2];
            uint8_t cmd = buffer[3];
            if (buffer[4] == CTRL_ETX)
            {
                if (cmd == CMD_PING)
                {
                    send_control(seq_sent, seq_recv, CMD_PING); // respond with ping
                }
            }
            index = 0;
            return false;
        }

        if (index == SQUID_BLOCK_SIZE)
        {
            index = 0;
            if (buffer[0] != STX || buffer[23] != ETX || calc_hash(buffer) != buffer[22])
                return false;
            memcpy(out_block, buffer, SQUID_BLOCK_SIZE);
            return true;
        }
    }
    return false;
}

void squid_init(const squid_platform_t *platform)
{
    plat = platform;
    state = STATE_STARTUP;
    seq_sent = 0;
    seq_recv = 0;
    retries = 0;
    last_tick = 0;
    memset(send_buffer, 0, SQUID_DATA_SIZE);
    memset(recv_buffer, 0, SQUID_DATA_SIZE);
    memset(last_sent, 0, SQUID_BLOCK_SIZE);
}

void squid_poll(void)
{
    uint8_t now = plat->get_tick();
    uint8_t block[SQUID_BLOCK_SIZE];

    if (recv_block(block))
    {
        uint8_t seq = block[1];
        uint8_t ack = block[2];
        uint8_t status = block[3];
        uint8_t type = block[4];
        const uint8_t *data = &block[6];

        if (type == TYPE_HELLO)
        {
            send_block(0, 0, STATUS_OK, TYPE_HELLO_ACK, hello_payload);
            return;
        }
        else if (type == TYPE_HELLO_ACK && state == STATE_STARTUP)
        {
            state = STATE_CONNECTED;
            retries = 0;
            return;
        }

        if (type == TYPE_DATA && seq != seq_recv)
        {
            memcpy(recv_buffer, data, SQUID_DATA_SIZE);
            seq_recv = seq;
        }

        if (ack == seq_sent && status == STATUS_OK)
        {
            state = STATE_CONNECTED;
            retries = 0;
        }
        else if (ack == seq_sent && status == STATUS_NAK)
        {
            state = STATE_WAITING_ACK;
        }
    }

    if ((timeout(now, last_tick, TIMEOUT_TICKS) && state != STATE_DISCONNECTED) || state == STATE_WAITING_ACK)
    {
        if (retries++ > MAX_RETRIES)
        {
            state = STATE_DISCONNECTED;
            return;
        }
        for (int i = 0; i < SQUID_BLOCK_SIZE; ++i)
            plat->send_char(last_sent[i]);
        last_tick = now;
        return;
    }

    if (state == STATE_STARTUP)
    {
        send_block(0, 0, STATUS_OK, TYPE_HELLO, hello_payload);
    }
    else if (state == STATE_CONNECTED)
    {
        seq_sent++;
        for (int i = 0; i < SQUID_DATA_SIZE; ++i)
            send_buffer[i]++;
        send_block(seq_sent, seq_recv, STATUS_OK, TYPE_DATA, send_buffer);
        state = STATE_WAITING_ACK;
    }
    else if (state == STATE_CONNECTED || state == STATE_WAITING_ACK)
    {
        // Periodic ping if needed
        if (timeout(now, last_tick, TIMEOUT_TICKS))
        {
            send_control(seq_sent, seq_recv, CMD_PING);
        }
    }
}

void squid_get_last_received(uint8_t *out_data)
{
    memcpy(out_data, recv_buffer, SQUID_DATA_SIZE);
}

bool squid_is_connected(void)
{
    return state == STATE_CONNECTED;
}
