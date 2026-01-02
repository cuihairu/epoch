#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#define EPOCH_AERON_FRAME_LENGTH 56

typedef struct epoch_aeron_transport epoch_aeron_transport_t;

typedef struct epoch_aeron_config
{
    const char *channel;
    int32_t stream_id;
    const char *aeron_directory;
    int32_t fragment_limit;
    int32_t offer_max_attempts;
}
epoch_aeron_config_t;

typedef struct epoch_aeron_stats
{
    int64_t sent_count;
    int64_t received_count;
    int64_t offer_back_pressure;
    int64_t offer_not_connected;
    int64_t offer_admin_action;
    int64_t offer_closed;
    int64_t offer_max_position;
    int64_t offer_failed;
}
epoch_aeron_stats_t;

epoch_aeron_transport_t *epoch_aeron_open(
    const epoch_aeron_config_t *config,
    char *error,
    size_t error_len);

int epoch_aeron_send(
    epoch_aeron_transport_t *transport,
    const uint8_t *frame,
    size_t frame_len,
    char *error,
    size_t error_len);

int epoch_aeron_poll(
    epoch_aeron_transport_t *transport,
    uint8_t *frames,
    size_t frame_capacity,
    size_t *out_count,
    char *error,
    size_t error_len);

int epoch_aeron_stats(epoch_aeron_transport_t *transport, epoch_aeron_stats_t *out_stats);

void epoch_aeron_close(epoch_aeron_transport_t *transport);

#ifdef __cplusplus
}
#endif
