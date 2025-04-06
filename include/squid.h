#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define SQUID_BLOCK_SIZE 24
#define SQUID_DATA_SIZE 16

    typedef struct
    {
        uint8_t data[SQUID_DATA_SIZE];
        uint8_t type;
    } squid_payload_t;

    typedef struct
    {
        // Platform-specific hooks
        int (*send_bytes)(const uint8_t *data, uint8_t len);
        int (*recv_bytes)(uint8_t *buffer, uint8_t len);
        uint64_t (*get_time_ms)(void);
    } squid_platform_t;

    void squid_init(const squid_platform_t *platform);
    void squid_poll(void); // Should be called regularly in main loop

#ifdef __cplusplus
}
#endif
