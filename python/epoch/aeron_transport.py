from __future__ import annotations

from dataclasses import dataclass
from typing import List

from .engine import Message
from .transport import Transport


@dataclass(frozen=True)
class AeronConfig:
    channel: str
    stream_id: int
    aeron_directory: str


class AeronTransport(Transport):
    def __init__(self, config: AeronConfig) -> None:
        self._config = config

    @property
    def config(self) -> AeronConfig:
        return self._config

    def send(self, message: Message) -> None:
        raise NotImplementedError("Aeron transport not linked")

    def poll(self, max_items: int) -> List[Message]:
        raise NotImplementedError("Aeron transport not linked")

    def close(self) -> None:
        return None
