#include "epoch_aeron.h"

#include <aeronc.h>
#include <concurrent/aeron_thread.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct epoch_aeron_transport
{
    aeron_context_t *context;
    aeron_t *client;
    aeron_publication_t *publication;
    aeron_subscription_t *subscription;
    epoch_aeron_config_t config;
    epoch_aeron_stats_t stats;
    int closed;
};

static void epoch_aeron_set_error(char *error, size_t error_len, const char *message)
{
    if (error == NULL || error_len == 0)
    {
        return;
    }
    if (message == NULL)
    {
        error[0] = '\0';
        return;
    }
    snprintf(error, error_len, "%s", message);
}

static void epoch_aeron_set_error_with_aeron(char *error, size_t error_len, const char *context)
{
    if (error == NULL || error_len == 0)
    {
        return;
    }
    const char *detail = aeron_errmsg();
    if (detail == NULL)
    {
        detail = "unknown";
    }
    snprintf(error, error_len, "%s: %s", context, detail);
}

static void epoch_aeron_apply_defaults(epoch_aeron_config_t *config)
{
    if (config->fragment_limit <= 0)
    {
        config->fragment_limit = 64;
    }
    if (config->offer_max_attempts <= 0)
    {
        config->offer_max_attempts = 10;
    }
}

static int epoch_aeron_init_publication(epoch_aeron_transport_t *transport, char *error, size_t error_len)
{
    aeron_async_add_publication_t *pub_async = NULL;
    if (aeron_async_add_publication(
            &pub_async, transport->client, transport->config.channel, transport->config.stream_id) < 0)
    {
        epoch_aeron_set_error_with_aeron(error, error_len, "aeron_async_add_publication failed");
        return -1;
    }
    while (1)
    {
        int poll_result = aeron_async_add_publication_poll(&transport->publication, pub_async);
        if (poll_result == 1)
        {
            break;
        }
        if (poll_result < 0)
        {
            epoch_aeron_set_error_with_aeron(error, error_len, "aeron_async_add_publication_poll failed");
            return -1;
        }
        sched_yield();
    }
    if (transport->publication == NULL)
    {
        epoch_aeron_set_error(error, error_len, "publication is null");
        return -1;
    }
    return 0;
}

static int epoch_aeron_init_subscription(epoch_aeron_transport_t *transport, char *error, size_t error_len)
{
    aeron_async_add_subscription_t *sub_async = NULL;
    if (aeron_async_add_subscription(
            &sub_async,
            transport->client,
            transport->config.channel,
            transport->config.stream_id,
            NULL,
            NULL,
            NULL,
            NULL) < 0)
    {
        epoch_aeron_set_error_with_aeron(error, error_len, "aeron_async_add_subscription failed");
        return -1;
    }
    while (1)
    {
        int poll_result = aeron_async_add_subscription_poll(&transport->subscription, sub_async);
        if (poll_result == 1)
        {
            break;
        }
        if (poll_result < 0)
        {
            epoch_aeron_set_error_with_aeron(error, error_len, "aeron_async_add_subscription_poll failed");
            return -1;
        }
        sched_yield();
    }
    if (transport->subscription == NULL)
    {
        epoch_aeron_set_error(error, error_len, "subscription is null");
        return -1;
    }
    return 0;
}

epoch_aeron_transport_t *epoch_aeron_open(
    const epoch_aeron_config_t *config,
    char *error,
    size_t error_len)
{
    if (config == NULL || config->channel == NULL)
    {
        epoch_aeron_set_error(error, error_len, "invalid config");
        return NULL;
    }
    epoch_aeron_transport_t *transport = calloc(1, sizeof(epoch_aeron_transport_t));
    if (transport == NULL)
    {
        epoch_aeron_set_error(error, error_len, "out of memory");
        return NULL;
    }

    transport->config = *config;
    transport->config.channel = strdup(config->channel);
    if (transport->config.channel == NULL)
    {
        epoch_aeron_set_error(error, error_len, "out of memory");
        epoch_aeron_close(transport);
        return NULL;
    }
    if (config->aeron_directory != NULL && config->aeron_directory[0] != '\0')
    {
        transport->config.aeron_directory = strdup(config->aeron_directory);
        if (transport->config.aeron_directory == NULL)
        {
            epoch_aeron_set_error(error, error_len, "out of memory");
            epoch_aeron_close(transport);
            return NULL;
        }
    }
    else
    {
        transport->config.aeron_directory = NULL;
    }
    epoch_aeron_apply_defaults(&transport->config);

    aeron_context_t *context = NULL;
    if (aeron_context_init(&context) < 0)
    {
        epoch_aeron_set_error_with_aeron(error, error_len, "aeron_context_init failed");
        epoch_aeron_close(transport);
        return NULL;
    }
    transport->context = context;

    if (transport->config.aeron_directory != NULL && transport->config.aeron_directory[0] != '\0')
    {
        if (aeron_context_set_dir(transport->context, transport->config.aeron_directory) < 0)
        {
            epoch_aeron_set_error_with_aeron(error, error_len, "aeron_context_set_dir failed");
            epoch_aeron_close(transport);
            return NULL;
        }
    }

    aeron_t *client = NULL;
    if (aeron_init(&client, transport->context) < 0)
    {
        epoch_aeron_set_error_with_aeron(error, error_len, "aeron_init failed");
        epoch_aeron_close(transport);
        return NULL;
    }
    transport->client = client;

    if (aeron_start(transport->client) < 0)
    {
        epoch_aeron_set_error_with_aeron(error, error_len, "aeron_start failed");
        epoch_aeron_close(transport);
        return NULL;
    }

    if (epoch_aeron_init_publication(transport, error, error_len) < 0)
    {
        epoch_aeron_close(transport);
        return NULL;
    }
    if (epoch_aeron_init_subscription(transport, error, error_len) < 0)
    {
        epoch_aeron_close(transport);
        return NULL;
    }

    return transport;
}

int epoch_aeron_send(
    epoch_aeron_transport_t *transport,
    const uint8_t *frame,
    size_t frame_len,
    char *error,
    size_t error_len)
{
    if (transport == NULL || transport->closed)
    {
        epoch_aeron_set_error(error, error_len, "transport closed");
        return -1;
    }
    if (frame == NULL || frame_len < EPOCH_AERON_FRAME_LENGTH)
    {
        epoch_aeron_set_error(error, error_len, "invalid frame");
        return -1;
    }

    int attempts = 0;
    while (attempts < transport->config.offer_max_attempts)
    {
        int64_t result = aeron_publication_offer(
            transport->publication, frame, EPOCH_AERON_FRAME_LENGTH, NULL, NULL);
        if (result >= 0)
        {
            transport->stats.sent_count++;
            return 0;
        }
        if (result == AERON_PUBLICATION_BACK_PRESSURED)
        {
            transport->stats.offer_back_pressure++;
        }
        else if (result == AERON_PUBLICATION_NOT_CONNECTED)
        {
            transport->stats.offer_not_connected++;
        }
        else if (result == AERON_PUBLICATION_ADMIN_ACTION)
        {
            transport->stats.offer_admin_action++;
        }
        else if (result == AERON_PUBLICATION_CLOSED)
        {
            transport->stats.offer_closed++;
        }
        else if (result == AERON_PUBLICATION_MAX_POSITION_EXCEEDED)
        {
            transport->stats.offer_max_position++;
        }
        else
        {
            transport->stats.offer_failed++;
        }
        attempts++;
        sched_yield();
    }

    epoch_aeron_set_error(error, error_len, "aeron offer failed");
    return -1;
}

typedef struct epoch_aeron_poll_context
{
    uint8_t *frames;
    size_t capacity;
    size_t count;
}
epoch_aeron_poll_context_t;

static void epoch_aeron_fragment_handler(void *clientd, const uint8_t *buffer, size_t length, aeron_header_t *header)
{
    (void)header;
    epoch_aeron_poll_context_t *ctx = (epoch_aeron_poll_context_t *)clientd;
    if (ctx->count >= ctx->capacity)
    {
        return;
    }
    if (length < EPOCH_AERON_FRAME_LENGTH)
    {
        return;
    }
    memcpy(ctx->frames + (ctx->count * EPOCH_AERON_FRAME_LENGTH), buffer, EPOCH_AERON_FRAME_LENGTH);
    ctx->count++;
}

int epoch_aeron_poll(
    epoch_aeron_transport_t *transport,
    uint8_t *frames,
    size_t frame_capacity,
    size_t *out_count,
    char *error,
    size_t error_len)
{
    if (out_count != NULL)
    {
        *out_count = 0;
    }
    if (transport == NULL || transport->closed)
    {
        epoch_aeron_set_error(error, error_len, "transport closed");
        return -1;
    }
    if (frames == NULL || frame_capacity == 0)
    {
        return 0;
    }

    epoch_aeron_poll_context_t context;
    context.frames = frames;
    context.capacity = frame_capacity;
    context.count = 0;

    size_t fragment_limit = frame_capacity;
    if (transport->config.fragment_limit > 0 && fragment_limit > (size_t)transport->config.fragment_limit)
    {
        fragment_limit = (size_t)transport->config.fragment_limit;
    }

    int fragments = aeron_subscription_poll(
        transport->subscription, epoch_aeron_fragment_handler, &context, fragment_limit);
    if (fragments < 0)
    {
        epoch_aeron_set_error_with_aeron(error, error_len, "aeron_subscription_poll failed");
        return -1;
    }

    if (out_count != NULL)
    {
        *out_count = context.count;
    }
    transport->stats.received_count += (int64_t)context.count;
    return 0;
}

int epoch_aeron_stats(epoch_aeron_transport_t *transport, epoch_aeron_stats_t *out_stats)
{
    if (transport == NULL || out_stats == NULL)
    {
        return -1;
    }
    *out_stats = transport->stats;
    return 0;
}

void epoch_aeron_close(epoch_aeron_transport_t *transport)
{
    if (transport == NULL || transport->closed)
    {
        return;
    }
    transport->closed = 1;

    if (transport->publication != NULL)
    {
        aeron_publication_close(transport->publication, NULL, NULL);
        transport->publication = NULL;
    }
    if (transport->subscription != NULL)
    {
        aeron_subscription_close(transport->subscription, NULL, NULL);
        transport->subscription = NULL;
    }
    if (transport->client != NULL)
    {
        aeron_close(transport->client);
        transport->client = NULL;
    }
    if (transport->context != NULL)
    {
        aeron_context_close(transport->context);
        transport->context = NULL;
    }

    free((void *)transport->config.channel);
    free((void *)transport->config.aeron_directory);
    free(transport);
}
