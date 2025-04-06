#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "squid.h"

static uint64_t get_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

static int send_bytes(const uint8_t *data, uint8_t len)
{
    // stub for now — log to console
    printf("[SEND] ");
    for (int i = 0; i < len; i++)
        printf("%02X ", data[i]);
    printf("\n");
    return len;
}

static int recv_bytes(uint8_t *buffer, uint8_t len)
{
    // stub for now — no input
    (void)buffer;
    return 0;
}

int main(void)
{
    squid_platform_t plat = {
        .send_bytes = send_bytes,
        .recv_bytes = recv_bytes,
        .get_time_ms = get_time_ms};

    squid_init(&plat);

    while (1)
    {
        squid_poll();
        usleep(1000); // simulate 1ms tick
    }

    return 0;
}
