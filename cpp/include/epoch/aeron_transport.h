#pragma once

#include "epoch/transport.h"

#include <aeronc.h>

#include <cstdint>
#include <string>

namespace epoch {

struct AeronConfig {
    std::string channel;
    std::int32_t stream_id;
    std::string aeron_directory;
    std::int32_t fragment_limit = 64;
    std::int32_t offer_max_attempts = 10;
};

struct AeronStats {
    std::int64_t sent_count = 0;
    std::int64_t received_count = 0;
    std::int64_t offer_back_pressure = 0;
    std::int64_t offer_not_connected = 0;
    std::int64_t offer_admin_action = 0;
    std::int64_t offer_closed = 0;
    std::int64_t offer_max_position = 0;
    std::int64_t offer_failed = 0;
};

class AeronTransport final : public Transport {
public:
    explicit AeronTransport(AeronConfig config);
    ~AeronTransport() override;

    AeronTransport(const AeronTransport &) = delete;
    AeronTransport &operator=(const AeronTransport &) = delete;
    AeronTransport(AeronTransport &&) = delete;
    AeronTransport &operator=(AeronTransport &&) = delete;

    void send(const Message &message) override;
    std::vector<Message> poll(std::size_t max) override;
    void close() override;

    const AeronConfig &config() const;
    const AeronStats &stats() const;

private:
    AeronConfig config_;
    AeronStats stats_;
    bool closed_ = false;
    aeron_context_t *context_ = nullptr;
    aeron_t *client_ = nullptr;
    aeron_publication_t *publication_ = nullptr;
    aeron_subscription_t *subscription_ = nullptr;
};

} // namespace epoch
