#pragma once

#include "epoch/transport.h"

#include <string>

namespace epoch {

struct AeronConfig {
    std::string channel;
    std::int32_t stream_id;
    std::string aeron_directory;
};

class AeronTransport final : public Transport {
public:
    explicit AeronTransport(AeronConfig config);

    void send(const Message &message) override;
    std::vector<Message> poll(std::size_t max) override;
    void close() override;

private:
    AeronConfig config_;
    bool closed_ = false;
};

} // namespace epoch
