from __future__ import annotations

from dataclasses import dataclass
from typing import Iterable, List

FNV_OFFSET_BASIS = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
FNV_MASK = 0xFFFFFFFFFFFFFFFF


@dataclass(frozen=True)
class Message:
    epoch: int
    channel_id: int
    source_id: int
    source_seq: int
    schema_id: int
    qos: int
    payload: int


@dataclass(frozen=True)
class EpochResult:
    epoch: int
    state: int
    hash: str


def fnv1a64_hex(value: str) -> str:
    h = FNV_OFFSET_BASIS
    for b in value.encode("utf-8"):
        h ^= b
        h = (h * FNV_PRIME) & FNV_MASK
    return f"{h:016x}"


def process_messages(messages: Iterable[Message]) -> List[EpochResult]:
    ordered = sorted(
        messages,
        key=lambda m: (m.epoch, m.channel_id, -m.qos, m.source_id, m.source_seq),
    )
    results: List[EpochResult] = []
    state = 0
    current_epoch = None

    for message in ordered:
        if current_epoch is None:
            current_epoch = message.epoch
        if message.epoch != current_epoch:
            results.append(
                EpochResult(
                    epoch=current_epoch,
                    state=state,
                    hash=fnv1a64_hex(f"state:{state}"),
                )
            )
            current_epoch = message.epoch
        state += message.payload

    if current_epoch is not None:
        results.append(
            EpochResult(
                epoch=current_epoch,
                state=state,
                hash=fnv1a64_hex(f"state:{state}"),
            )
        )

    return results
