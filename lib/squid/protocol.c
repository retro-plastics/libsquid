#include "squid.h"
#include "protocol_internal.h"

static const squid_platform_t *plat;

void squid_init(const squid_platform_t *platform)
{
    plat = platform;
}

void squid_poll(void)
{
    // will call plat->recv_bytes() and plat->send_bytes()
}
