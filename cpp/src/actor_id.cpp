#include "epoch/actor_id.h"

#include <stdexcept>

namespace epoch {

namespace {
constexpr std::uint32_t kRegionBits = 10;
constexpr std::uint32_t kServerBits = 12;
constexpr std::uint32_t kProcessTypeBits = 6;
constexpr std::uint32_t kProcessIndexBits = 10;
constexpr std::uint32_t kActorIndexBits = 26;

constexpr std::uint64_t kRegionMax = (1ULL << kRegionBits) - 1;
constexpr std::uint64_t kServerMax = (1ULL << kServerBits) - 1;
constexpr std::uint64_t kProcessTypeMax = (1ULL << kProcessTypeBits) - 1;
constexpr std::uint64_t kProcessIndexMax = (1ULL << kProcessIndexBits) - 1;
constexpr std::uint64_t kActorIndexMax = (1ULL << kActorIndexBits) - 1;

constexpr std::uint32_t kActorIndexShift = 0;
constexpr std::uint32_t kProcessIndexShift = kActorIndexShift + kActorIndexBits;
constexpr std::uint32_t kProcessTypeShift = kProcessIndexShift + kProcessIndexBits;
constexpr std::uint32_t kServerShift = kProcessTypeShift + kProcessTypeBits;
constexpr std::uint32_t kRegionShift = kServerShift + kServerBits;
} // namespace

std::uint64_t DefaultActorIdCodec::encode(const ActorIdParts &parts) const
{
    if (parts.region > kRegionMax || parts.server > kServerMax ||
        parts.process_type > kProcessTypeMax || parts.process_index > kProcessIndexMax ||
        parts.actor_index > kActorIndexMax)
    {
        throw std::out_of_range("ActorIdParts out of range");
    }

    return (static_cast<std::uint64_t>(parts.region) << kRegionShift) |
           (static_cast<std::uint64_t>(parts.server) << kServerShift) |
           (static_cast<std::uint64_t>(parts.process_type) << kProcessTypeShift) |
           (static_cast<std::uint64_t>(parts.process_index) << kProcessIndexShift) |
           (static_cast<std::uint64_t>(parts.actor_index) << kActorIndexShift);
}

ActorIdParts DefaultActorIdCodec::decode(std::uint64_t value) const
{
    ActorIdParts parts{};
    parts.region = static_cast<std::uint16_t>((value >> kRegionShift) & kRegionMax);
    parts.server = static_cast<std::uint16_t>((value >> kServerShift) & kServerMax);
    parts.process_type = static_cast<std::uint8_t>((value >> kProcessTypeShift) & kProcessTypeMax);
    parts.process_index = static_cast<std::uint16_t>((value >> kProcessIndexShift) & kProcessIndexMax);
    parts.actor_index = static_cast<std::uint32_t>((value >> kActorIndexShift) & kActorIndexMax);
    return parts;
}

const char *DefaultActorIdCodec::name() const
{
    return "default";
}

const ActorIdCodec &default_actor_id_codec()
{
    static DefaultActorIdCodec codec;
    return codec;
}

std::uint64_t encode_actor_id(const ActorIdParts &parts, const ActorIdCodec &codec)
{
    return codec.encode(parts);
}

ActorIdParts decode_actor_id(std::uint64_t value, const ActorIdCodec &codec)
{
    return codec.decode(value);
}

} // namespace epoch
