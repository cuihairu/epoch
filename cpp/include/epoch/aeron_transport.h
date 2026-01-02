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

namespace detail {

struct AeronHooks {
    int (*context_init)(aeron_context_t **);
    int (*context_set_dir)(aeron_context_t *, const char *);
    int (*init)(aeron_t **, aeron_context_t *);
    int (*start)(aeron_t *);
    int (*async_add_publication)(aeron_async_add_publication_t **, aeron_t *, const char *, int32_t);
    int (*async_add_publication_poll)(aeron_publication_t **, aeron_async_add_publication_t *);
    int (*async_add_subscription)(aeron_async_add_subscription_t **, aeron_t *, const char *, int32_t,
                                  aeron_on_available_image_t, void *, aeron_on_unavailable_image_t, void *);
    int (*async_add_subscription_poll)(aeron_subscription_t **, aeron_async_add_subscription_t *);
    int64_t (*publication_offer)(aeron_publication_t *, const uint8_t *, size_t,
                                 aeron_reserved_value_supplier_t, void *);
    int (*subscription_poll)(aeron_subscription_t *, aeron_fragment_handler_t, void *, size_t);
    int (*publication_close)(aeron_publication_t *, aeron_notification_t, void *);
    int (*subscription_close)(aeron_subscription_t *, aeron_notification_t, void *);
    int (*close)(aeron_t *);
    int (*context_close)(aeron_context_t *);
    const char *(*errmsg)();
};

AeronHooks &aeron_hooks();

} // namespace detail

#ifdef EPOCH_TESTING
namespace test {

detail::AeronHooks &aeron_hooks();
void reset_aeron_hooks();

} // namespace test
#endif

} // namespace epoch
