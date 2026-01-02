from __future__ import annotations

from dataclasses import dataclass
from typing import List, Optional

import ctypes
import os
import struct
import sys

from .engine import Message
from .transport import Transport


FRAME_VERSION = 1
FRAME_LENGTH = 56
_FRAME_STRUCT = struct.Struct("<BB6xqqqqqq")


@dataclass(frozen=True)
class AeronConfig:
    channel: str
    stream_id: int
    aeron_directory: str
    fragment_limit: int = 64
    offer_max_attempts: int = 10


@dataclass(frozen=True)
class AeronStats:
    sent_count: int
    received_count: int
    offer_back_pressure: int
    offer_not_connected: int
    offer_admin_action: int
    offer_closed: int
    offer_max_position: int
    offer_failed: int


def encode_aeron_frame(message: Message) -> bytes:
    return _FRAME_STRUCT.pack(
        FRAME_VERSION,
        message.qos,
        message.epoch,
        message.channel_id,
        message.source_id,
        message.source_seq,
        message.schema_id,
        message.payload,
    )


def decode_aeron_frame(buffer: bytes) -> Message:
    if len(buffer) < FRAME_LENGTH:
        raise ValueError("aeron frame too short")
    version, qos, epoch, channel_id, source_id, source_seq, schema_id, payload = _FRAME_STRUCT.unpack_from(buffer)
    if version != FRAME_VERSION:
        raise ValueError("unsupported aeron frame version")
    return Message(
        epoch=epoch,
        channel_id=channel_id,
        source_id=source_id,
        source_seq=source_seq,
        schema_id=schema_id,
        qos=qos,
        payload=payload,
    )


class _NativeAeron:
    def __init__(self, library_path: Optional[str] = None) -> None:
        lib_path = library_path or _find_native_library()
        if not lib_path:
            raise RuntimeError("epoch_aeron native library not found")
        self._lib = ctypes.CDLL(lib_path)
        self._lib.epoch_aeron_open.restype = ctypes.c_void_p
        self._lib.epoch_aeron_open.argtypes = [ctypes.POINTER(_CConfig), ctypes.c_char_p, ctypes.c_size_t]
        self._lib.epoch_aeron_send.restype = ctypes.c_int
        self._lib.epoch_aeron_send.argtypes = [
            ctypes.c_void_p,
            ctypes.POINTER(ctypes.c_uint8),
            ctypes.c_size_t,
            ctypes.c_char_p,
            ctypes.c_size_t,
        ]
        self._lib.epoch_aeron_poll.restype = ctypes.c_int
        self._lib.epoch_aeron_poll.argtypes = [
            ctypes.c_void_p,
            ctypes.POINTER(ctypes.c_uint8),
            ctypes.c_size_t,
            ctypes.POINTER(ctypes.c_size_t),
            ctypes.c_char_p,
            ctypes.c_size_t,
        ]
        self._lib.epoch_aeron_stats.restype = ctypes.c_int
        self._lib.epoch_aeron_stats.argtypes = [ctypes.c_void_p, ctypes.POINTER(_CStats)]
        self._lib.epoch_aeron_close.argtypes = [ctypes.c_void_p]

    def open(self, config: AeronConfig) -> ctypes.c_void_p:
        err_buf = ctypes.create_string_buffer(256)
        c_config = _CConfig.from_config(config)
        handle = self._lib.epoch_aeron_open(ctypes.byref(c_config), err_buf, ctypes.sizeof(err_buf))
        if not handle:
            raise RuntimeError(err_buf.value.decode("utf-8", errors="ignore"))
        return ctypes.c_void_p(handle)

    def send(self, handle: ctypes.c_void_p, frame: bytes) -> None:
        err_buf = ctypes.create_string_buffer(256)
        data = (ctypes.c_uint8 * len(frame)).from_buffer_copy(frame)
        result = self._lib.epoch_aeron_send(handle, data, len(frame), err_buf, ctypes.sizeof(err_buf))
        if result < 0:
            raise RuntimeError(err_buf.value.decode("utf-8", errors="ignore"))

    def poll(self, handle: ctypes.c_void_p, max_items: int) -> List[bytes]:
        if max_items <= 0:
            return []
        err_buf = ctypes.create_string_buffer(256)
        total_len = FRAME_LENGTH * max_items
        buffer = (ctypes.c_uint8 * total_len)()
        out_count = ctypes.c_size_t()
        result = self._lib.epoch_aeron_poll(
            handle,
            buffer,
            max_items,
            ctypes.byref(out_count),
            err_buf,
            ctypes.sizeof(err_buf),
        )
        if result < 0:
            raise RuntimeError(err_buf.value.decode("utf-8", errors="ignore"))
        frames = []
        for i in range(out_count.value):
            start = i * FRAME_LENGTH
            frames.append(bytes(buffer[start : start + FRAME_LENGTH]))
        return frames

    def stats(self, handle: ctypes.c_void_p) -> AeronStats:
        c_stats = _CStats()
        if self._lib.epoch_aeron_stats(handle, ctypes.byref(c_stats)) < 0:
            return AeronStats(0, 0, 0, 0, 0, 0, 0, 0)
        return c_stats.to_stats()

    def close(self, handle: ctypes.c_void_p) -> None:
        if handle:
            self._lib.epoch_aeron_close(handle)


class _CConfig(ctypes.Structure):
    _fields_ = [
        ("channel", ctypes.c_char_p),
        ("stream_id", ctypes.c_int32),
        ("aeron_directory", ctypes.c_char_p),
        ("fragment_limit", ctypes.c_int32),
        ("offer_max_attempts", ctypes.c_int32),
    ]

    @staticmethod
    def from_config(config: AeronConfig) -> "_CConfig":
        return _CConfig(
            channel=config.channel.encode("utf-8"),
            stream_id=config.stream_id,
            aeron_directory=config.aeron_directory.encode("utf-8") if config.aeron_directory else None,
            fragment_limit=config.fragment_limit,
            offer_max_attempts=config.offer_max_attempts,
        )


class _CStats(ctypes.Structure):
    _fields_ = [
        ("sent_count", ctypes.c_int64),
        ("received_count", ctypes.c_int64),
        ("offer_back_pressure", ctypes.c_int64),
        ("offer_not_connected", ctypes.c_int64),
        ("offer_admin_action", ctypes.c_int64),
        ("offer_closed", ctypes.c_int64),
        ("offer_max_position", ctypes.c_int64),
        ("offer_failed", ctypes.c_int64),
    ]

    def to_stats(self) -> AeronStats:
        return AeronStats(
            sent_count=self.sent_count,
            received_count=self.received_count,
            offer_back_pressure=self.offer_back_pressure,
            offer_not_connected=self.offer_not_connected,
            offer_admin_action=self.offer_admin_action,
            offer_closed=self.offer_closed,
            offer_max_position=self.offer_max_position,
            offer_failed=self.offer_failed,
        )


class AeronTransport(Transport):
    def __init__(self, config: AeronConfig, native: Optional[_NativeAeron] = None) -> None:
        self._config = config
        self._native = native or _NativeAeron()
        self._handle = self._native.open(config)
        self._closed = False

    @property
    def config(self) -> AeronConfig:
        return self._config

    def stats(self) -> AeronStats:
        if self._closed:
            return AeronStats(0, 0, 0, 0, 0, 0, 0, 0)
        return self._native.stats(self._handle)

    def send(self, message: Message) -> None:
        if self._closed:
            raise RuntimeError("aeron transport closed")
        frame = encode_aeron_frame(message)
        self._native.send(self._handle, frame)

    def poll(self, max_items: int) -> List[Message]:
        if self._closed:
            return []
        frames = self._native.poll(self._handle, max_items)
        return [decode_aeron_frame(frame) for frame in frames]

    def close(self) -> None:
        if self._closed:
            return None
        self._closed = True
        self._native.close(self._handle)
        return None


def _find_native_library() -> Optional[str]:
    env_path = os.getenv("EPOCH_AERON_LIBRARY")
    if env_path:
        return env_path

    filename = "libepoch_aeron.so"
    if sys.platform == "darwin":
        filename = "libepoch_aeron.dylib"
    elif sys.platform.startswith("win"):
        filename = "epoch_aeron.dll"

    current = os.path.abspath(os.path.dirname(__file__))
    for _ in range(6):
        candidate = os.path.join(current, "..", "native", "build", filename)
        candidate = os.path.abspath(candidate)
        if os.path.exists(candidate):
            return candidate
        parent = os.path.dirname(current)
        if parent == current:
            break
        current = parent

    try:
        import ctypes.util

        found = ctypes.util.find_library("epoch_aeron")
        if found:
            return found
    except Exception:
        return None

    return None
