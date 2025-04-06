#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include "squid.h"

// Simulated circular input buffer (empty in this example)
#define INPUT_BUFFER_SIZE 256
static uint8_t input_buffer[INPUT_BUFFER_SIZE];
static int input_head = 0;
static int input_tail = 0;

static int send_char(uint8_t c)
{
    // For now, print as hex to stdout (simulate send)
    printf("[SEND] %02X\n", c);
    return 0;
}

static int recv_char(void)
{
    // Simulate empty buffer
    if (input_head == input_tail)
        return -1;
    uint8_t c = input_buffer[input_tail];
    input_tail = (input_tail + 1) % INPUT_BUFFER_SIZE;
    return c;
}

// Return a tick counter at ~50Hz, using clock time
static uint8_t get_tick(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint8_t)((ts.tv_nsec / 20000000) & 0xFF); // ~50Hz tick
}

int main(void)
{
    squid_platform_t plat = {
        .send_char = send_char,
        .recv_char = recv_char,
        .get_tick = get_tick};

    squid_init(&plat);

    while (1)
    {
        squid_poll();

        if (squid_is_connected())
        {
            uint8_t data[SQUID_DATA_SIZE];
            squid_get_last_received(data);
            printf("[RECV] ");
            for (int i = 0; i < SQUID_DATA_SIZE; ++i)
                printf("%02X ", data[i]);
            printf("\n");
        }

        usleep(20000); // 50Hz loop
    }

    return 0;
}
