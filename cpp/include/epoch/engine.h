#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace epoch {

struct Message {
    std::int64_t epoch;
    std::int64_t channel_id;
    std::int64_t source_id;
    std::int64_t source_seq;
    std::int64_t schema_id;
    std::int64_t payload;
};

struct EpochResult {
    std::int64_t epoch;
    std::int64_t state;
    std::string hash;
};

std::string fnv1a64_hex(const std::string &input);
std::vector<EpochResult> process_messages(std::vector<Message> messages);

} // namespace epoch
