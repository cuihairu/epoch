from __future__ import annotations

from dataclasses import dataclass
from typing import Protocol


@dataclass(frozen=True)
class ActorIdParts:
    region: int
    server: int
    process_type: int
    process_index: int
    actor_index: int


class ActorIdCodec(Protocol):
    def encode(self, parts: ActorIdParts) -> int:
        ...

    def decode(self, value: int) -> ActorIdParts:
        ...

    @property
    def name(self) -> str:
        ...


class DefaultActorIdCodec:
    _region_bits = 10
    _server_bits = 12
    _process_type_bits = 6
    _process_index_bits = 10
    _actor_index_bits = 26

    _region_max = (1 << _region_bits) - 1
    _server_max = (1 << _server_bits) - 1
    _process_type_max = (1 << _process_type_bits) - 1
    _process_index_max = (1 << _process_index_bits) - 1
    _actor_index_max = (1 << _actor_index_bits) - 1

    _actor_index_shift = 0
    _process_index_shift = _actor_index_shift + _actor_index_bits
    _process_type_shift = _process_index_shift + _process_index_bits
    _server_shift = _process_type_shift + _process_type_bits
    _region_shift = _server_shift + _server_bits

    @property
    def name(self) -> str:
        return "default"

    def encode(self, parts: ActorIdParts) -> int:
        if (parts.region < 0 or parts.region > self._region_max or
            parts.server < 0 or parts.server > self._server_max or
            parts.process_type < 0 or parts.process_type > self._process_type_max or
            parts.process_index < 0 or parts.process_index > self._process_index_max or
            parts.actor_index < 0 or parts.actor_index > self._actor_index_max):
            raise ValueError("ActorId parts out of range")

        return (
            (parts.region << self._region_shift)
            | (parts.server << self._server_shift)
            | (parts.process_type << self._process_type_shift)
            | (parts.process_index << self._process_index_shift)
            | (parts.actor_index << self._actor_index_shift)
        )

    def decode(self, value: int) -> ActorIdParts:
        return ActorIdParts(
            region=(value >> self._region_shift) & self._region_max,
            server=(value >> self._server_shift) & self._server_max,
            process_type=(value >> self._process_type_shift) & self._process_type_max,
            process_index=(value >> self._process_index_shift) & self._process_index_max,
            actor_index=(value >> self._actor_index_shift) & self._actor_index_max,
        )


default_actor_id_codec = DefaultActorIdCodec()


def encode_actor_id(parts: ActorIdParts, codec: ActorIdCodec = default_actor_id_codec) -> int:
    return codec.encode(parts)


def decode_actor_id(value: int, codec: ActorIdCodec = default_actor_id_codec) -> ActorIdParts:
    return codec.decode(value)
