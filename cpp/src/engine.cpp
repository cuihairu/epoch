#include "epoch/engine.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace epoch {

namespace {
constexpr std::uint64_t kFnvOffsetBasis = 0xcbf29ce484222325ULL;
constexpr std::uint64_t kFnvPrime = 0x100000001b3ULL;
} // namespace

std::string fnv1a64_hex(const std::string &input)
{
    std::uint64_t hash = kFnvOffsetBasis;
    for (unsigned char byte : input)
    {
        hash ^= static_cast<std::uint64_t>(byte);
        hash *= kFnvPrime;
    }

    std::ostringstream out;
    out << std::hex << std::nouppercase << std::setw(16) << std::setfill('0') << hash;
    return out.str();
}

std::vector<EpochResult> process_messages(std::vector<Message> messages)
{
    std::sort(messages.begin(), messages.end(), [](const Message &a, const Message &b) {
        if (a.epoch != b.epoch)
        {
            return a.epoch < b.epoch;
        }
        if (a.channel_id != b.channel_id)
        {
            return a.channel_id < b.channel_id;
        }
        if (a.source_id != b.source_id)
        {
            return a.source_id < b.source_id;
        }
        return a.source_seq < b.source_seq;
    });

    std::vector<EpochResult> results;
    std::int64_t current_epoch = 0;
    std::int64_t state = 0;
    bool has_epoch = false;

    for (const auto &msg : messages)
    {
        if (!has_epoch)
        {
            current_epoch = msg.epoch;
            has_epoch = true;
        }
        if (msg.epoch != current_epoch)
        {
            std::string hash = fnv1a64_hex("state:" + std::to_string(state));
            results.push_back({current_epoch, state, hash});
            current_epoch = msg.epoch;
        }
        state += msg.payload;
    }

    if (has_epoch)
    {
        std::string hash = fnv1a64_hex("state:" + std::to_string(state));
        results.push_back({current_epoch, state, hash});
    }

    return results;
}

} // namespace epoch
