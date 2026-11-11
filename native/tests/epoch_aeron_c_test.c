#include "epoch_aeron.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;

#define CHECK(condition, message) \
    do \
    { \
        if (!(condition)) \
        { \
            fprintf(stderr, "FAIL: %s\n", message); \
            failures++; \
        } \
    } while (0)

static void test_open_rejects_null_channel(void)
{
    epoch_aeron_config_t config;
    memset(&config, 0, sizeof(config));
    config.channel = NULL;
    config.stream_id = 1;

    char error[256];
    epoch_aeron_transport_t *transport = epoch_aeron_open(&config, error, sizeof(error));
    CHECK(transport == NULL, "open with null channel should fail");
    CHECK(strstr(error, "invalid config") != NULL, "invalid config error message missing");
}

int main(void)
{
    test_open_rejects_null_channel();

    if (failures > 0)
    {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    printf("all native aeron C tests passed\n");
    return 0;
}
