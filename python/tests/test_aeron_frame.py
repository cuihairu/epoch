import unittest

from epoch.aeron_transport import (
    AeronConfig,
    AeronStats,
    AeronTransport,
    decode_aeron_frame,
    encode_aeron_frame,
)
from epoch.engine import Message


class FakeNative:
    def __init__(self) -> None:
        self.frames = []

    def open(self, config: AeronConfig) -> object:
        return object()

    def send(self, handle: object, frame: bytes) -> None:
        self.frames.append(frame)

    def poll(self, handle: object, max_items: int) -> list[bytes]:
        out = self.frames[:max_items]
        self.frames = self.frames[max_items:]
        return out

    def stats(self, handle: object) -> AeronStats:
        return AeronStats(0, 0, 0, 0, 0, 0, 0, 0)

    def close(self, handle: object) -> None:
        return None


class AeronFrameTests(unittest.TestCase):
    def test_round_trip(self) -> None:
        message = Message(1, 2, 3, 4, 5, 6, -7)
        frame = encode_aeron_frame(message)
        decoded = decode_aeron_frame(frame)
        self.assertEqual(decoded, message)

    def test_rejects_short_frame(self) -> None:
        with self.assertRaises(ValueError):
            decode_aeron_frame(b"\x01\x00")

    def test_transport_with_fake_native(self) -> None:
        native = FakeNative()
        config = AeronConfig("aeron:ipc", 1, "")
        transport = AeronTransport(config, native=native)
        transport.send(Message(1, 1, 1, 1, 1, 1, 9))
        transport.send(Message(1, 1, 1, 2, 1, 1, 10))
        batch = transport.poll(1)
        self.assertEqual([m.payload for m in batch], [9])
        self.assertEqual([m.payload for m in transport.poll(5)], [10])
        transport.close()
        self.assertEqual(transport.poll(1), [])


if __name__ == "__main__":
    unittest.main()
