from __future__ import annotations

from collections import deque
from typing import Deque, Iterable, List, Protocol

from .engine import Message


class Transport(Protocol):
    def send(self, message: Message) -> None:
        ...

    def poll(self, max_items: int) -> List[Message]:
        ...

    def close(self) -> None:
        ...


class InMemoryTransport:
    def __init__(self) -> None:
        self._queue: Deque[Message] = deque()

    def send(self, message: Message) -> None:
        self._queue.append(message)

    def poll(self, max_items: int) -> List[Message]:
        if max_items <= 0:
            return []
        items: List[Message] = []
        while self._queue and len(items) < max_items:
            items.append(self._queue.popleft())
        return items

    def close(self) -> None:
        self._queue.clear()
