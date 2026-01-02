#include "epoch/aeron_transport.h"

extern "C" {
#include <aeronc.h>
}

#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>
#include <string>
#include <thread>

namespace epoch {

namespace {

constexpr std::uint8_t kFrameVersion = 1;
constexpr std::size_t kFrameLength = 56;
constexpr std::size_t kOffsetVersion = 0;
constexpr std::size_t kOffsetQos = 1;
constexpr std::size_t kOffsetEpoch = 8;
constexpr std::size_t kOffsetChannel = 16;
constexpr std::size_t kOffsetSource = 24;
constexpr std::size_t kOffsetSourceSeq = 32;
constexpr std::size_t kOffsetSchema = 40;
constexpr std::size_t kOffsetPayload = 48;

void write_i64(std::uint8_t *buffer, std::size_t offset, std::int64_t value)
{
    std::memcpy(buffer + offset, &value, sizeof(value));
}

std::int64_t read_i64(const std::uint8_t *buffer, std::size_t offset)
{
    std::int64_t value = 0;
    std::memcpy(&value, buffer + offset, sizeof(value));
    return value;
}

void encode_message(std::uint8_t *buffer, const Message &message)
{
    buffer[kOffsetVersion] = kFrameVersion;
    buffer[kOffsetQos] = message.qos;
    std::memset(buffer + 2, 0, 6);
    write_i64(buffer, kOffsetEpoch, message.epoch);
    write_i64(buffer, kOffsetChannel, message.channel_id);
    write_i64(buffer, kOffsetSource, message.source_id);
    write_i64(buffer, kOffsetSourceSeq, message.source_seq);
    write_i64(buffer, kOffsetSchema, message.schema_id);
    write_i64(buffer, kOffsetPayload, message.payload);
}

bool decode_message(const std::uint8_t *buffer, std::size_t length, Message &message)
{
    if (length < kFrameLength)
    {
        return false;
    }
    auto version = buffer[kOffsetVersion];
    if (version != kFrameVersion)
    {
        return false;
    }
    message.qos = buffer[kOffsetQos];
    message.epoch = read_i64(buffer, kOffsetEpoch);
    message.channel_id = read_i64(buffer, kOffsetChannel);
    message.source_id = read_i64(buffer, kOffsetSource);
    message.source_seq = read_i64(buffer, kOffsetSourceSeq);
    message.schema_id = read_i64(buffer, kOffsetSchema);
    message.payload = read_i64(buffer, kOffsetPayload);
    return true;
}

void throw_if_error(int result, const char *context)
{
    if (result < 0)
    {
        throw std::runtime_error(std::string(context) + ": " + aeron_errmsg());
    }
}

void throw_if_null(void *ptr, const char *context)
{
    if (ptr == nullptr)
    {
        throw std::runtime_error(std::string(context) + ": " + aeron_errmsg());
    }
}

} // namespace

AeronTransport::AeronTransport(AeronConfig config) : config_(std::move(config))
{
    if (config_.fragment_limit <= 0)
    {
        config_.fragment_limit = 64;
    }
    if (config_.offer_max_attempts <= 0)
    {
        config_.offer_max_attempts = 10;
    }

    try
    {
        aeron_context_t *context = nullptr;
        throw_if_error(aeron_context_init(&context), "aeron_context_init failed");
        if (!config_.aeron_directory.empty())
        {
            throw_if_error(aeron_context_set_dir(context, config_.aeron_directory.c_str()),
                           "aeron_context_set_dir failed");
        }
        context_ = context;

        aeron_t *client = nullptr;
        throw_if_error(aeron_init(&client, context_), "aeron_init failed");
        throw_if_error(aeron_start(client), "aeron_start failed");
        client_ = client;

        aeron_async_add_publication_t *pub_async = nullptr;
        throw_if_error(aeron_async_add_publication(&pub_async, client_, config_.channel.c_str(), config_.stream_id),
                       "aeron_async_add_publication failed");
        while (true)
        {
            int poll_result = aeron_async_add_publication_poll(&publication_, pub_async);
            if (poll_result == 1)
            {
                break;
            }
            throw_if_error(poll_result, "aeron_async_add_publication_poll failed");
            std::this_thread::yield();
        }
        throw_if_null(publication_, "publication is null");

        aeron_async_add_subscription_t *sub_async = nullptr;
        throw_if_error(
            aeron_async_add_subscription(
                &sub_async, client_, config_.channel.c_str(), config_.stream_id, nullptr, nullptr, nullptr, nullptr),
            "aeron_async_add_subscription failed");
        while (true)
        {
            int poll_result = aeron_async_add_subscription_poll(&subscription_, sub_async);
            if (poll_result == 1)
            {
                break;
            }
            throw_if_error(poll_result, "aeron_async_add_subscription_poll failed");
            std::this_thread::yield();
        }
        throw_if_null(subscription_, "subscription is null");
    }
    catch (...)
    {
        close();
        throw;
    }
}

AeronTransport::~AeronTransport()
{
    close();
}

void AeronTransport::send(const Message &message)
{
    if (closed_)
    {
        throw std::runtime_error("Aeron transport is closed");
    }
    std::array<std::uint8_t, kFrameLength> buffer{};
    encode_message(buffer.data(), message);

    int attempts = 0;
    std::int64_t result = 0;
    do
    {
        result = aeron_publication_offer(publication_, buffer.data(), buffer.size(), nullptr, nullptr);
        if (result >= 0)
        {
            stats_.sent_count++;
            return;
        }
        if (result == AERON_PUBLICATION_BACK_PRESSURED)
        {
            stats_.offer_back_pressure++;
        }
        else if (result == AERON_PUBLICATION_NOT_CONNECTED)
        {
            stats_.offer_not_connected++;
        }
        else if (result == AERON_PUBLICATION_ADMIN_ACTION)
        {
            stats_.offer_admin_action++;
        }
        else if (result == AERON_PUBLICATION_CLOSED)
        {
            stats_.offer_closed++;
        }
        else if (result == AERON_PUBLICATION_MAX_POSITION_EXCEEDED)
        {
            stats_.offer_max_position++;
        }
        else
        {
            stats_.offer_failed++;
        }
        attempts++;
        std::this_thread::yield();
    } while (attempts < std::max(1, config_.offer_max_attempts));

    throw std::runtime_error("Aeron offer failed");
}

std::vector<Message> AeronTransport::poll(std::size_t max)
{
    if (closed_ || max == 0)
    {
        return {};
    }

    std::vector<Message> out;
    std::size_t limit = std::min(max, static_cast<std::size_t>(std::max(1, config_.fragment_limit)));
    out.reserve(limit);

    struct PollContext {
        std::vector<Message> *out;
        AeronStats *stats;
    } context{&out, &stats_};

    auto handler = [](void *clientd, const std::uint8_t *buffer, std::size_t length, aeron_header_t *) {
        auto *ctx = static_cast<PollContext *>(clientd);
        Message message{};
        if (!decode_message(buffer, length, message))
        {
            return;
        }
        ctx->out->push_back(message);
        ctx->stats->received_count++;
    };

    int fragments = aeron_subscription_poll(subscription_, handler, &context, limit);
    throw_if_error(fragments, "aeron_subscription_poll failed");
    return out;
}

void AeronTransport::close()
{
    if (closed_)
    {
        return;
    }
    closed_ = true;

    if (publication_ != nullptr)
    {
        aeron_publication_close(publication_, nullptr, nullptr);
        publication_ = nullptr;
    }
    if (subscription_ != nullptr)
    {
        aeron_subscription_close(subscription_, nullptr, nullptr);
        subscription_ = nullptr;
    }
    if (client_ != nullptr)
    {
        aeron_close(client_);
        client_ = nullptr;
    }
    if (context_ != nullptr)
    {
        aeron_context_close(context_);
        context_ = nullptr;
    }
}

const AeronConfig &AeronTransport::config() const
{
    return config_;
}

const AeronStats &AeronTransport::stats() const
{
    return stats_;
}

} // namespace epoch
