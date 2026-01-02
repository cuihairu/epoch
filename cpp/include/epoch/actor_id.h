#pragma once

#include <cstdint>

namespace epoch {

struct ActorIdParts {
    std::uint16_t region;
    std::uint16_t server;
    std::uint8_t process_type;
    std::uint16_t process_index;
    std::uint32_t actor_index;
};

class ActorIdCodec {
public:
    virtual ~ActorIdCodec() = default;
    virtual std::uint64_t encode(const ActorIdParts &parts) const = 0;
    virtual ActorIdParts decode(std::uint64_t value) const = 0;
    virtual const char *name() const = 0;
};

class DefaultActorIdCodec final : public ActorIdCodec {
public:
    std::uint64_t encode(const ActorIdParts &parts) const override;
    ActorIdParts decode(std::uint64_t value) const override;
    const char *name() const override;
};

const ActorIdCodec &default_actor_id_codec();
std::uint64_t encode_actor_id(const ActorIdParts &parts, const ActorIdCodec &codec = default_actor_id_codec());
ActorIdParts decode_actor_id(std::uint64_t value, const ActorIdCodec &codec = default_actor_id_codec());

} // namespace epoch
