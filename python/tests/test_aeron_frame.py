import unittest
import os
import sys
from unittest import mock

from epoch.aeron_transport import (
    AeronConfig,
    AeronStats,
    AeronTransport,
    _NativeAeron,
    _find_native_library,
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


class ErrorFakeNative(FakeNative):
    def open(self, config: AeronConfig) -> object:
        return object()

    def send(self, handle: object, frame: bytes) -> None:
        raise RuntimeError("send failed")

    def poll(self, handle: object, max_items: int) -> list[bytes]:
        raise RuntimeError("poll failed")

    def stats(self, handle: object) -> AeronStats:
        return AeronStats(1, 2, 3, 4, 5, 6, 7, 8)


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

    def test_decode_rejects_unsupported_version(self) -> None:
        frame = encode_aeron_frame(Message(1, 2, 3, 4, 5, 6, 7))
        frame = b"\x02" + frame[1:]
        with self.assertRaisesRegex(ValueError, "unsupported"):
            decode_aeron_frame(frame)

    def test_transport_configuration_and_closed_errors(self) -> None:
        config = AeronConfig("aeron:ipc", 2, "", 0, 0)
        self.assertEqual(config.fragment_limit, 0)
        self.assertEqual(config.offer_max_attempts, 0)

        native = ErrorFakeNative()
        transport = AeronTransport(config, native=native)
        self.assertEqual(transport.config, config)
        self.assertEqual(transport.stats(), AeronStats(1, 2, 3, 4, 5, 6, 7, 8))
        with self.assertRaisesRegex(RuntimeError, "send failed"):
            transport.send(Message(1, 1, 1, 1, 1, 1, 1))
        with self.assertRaisesRegex(RuntimeError, "poll failed"):
            transport.poll(1)
        transport.close()
        transport.close()
        with self.assertRaisesRegex(RuntimeError, "closed"):
            transport.send(Message(1, 1, 1, 1, 1, 1, 1))
        self.assertEqual(transport.poll(1), [])
        self.assertEqual(transport.stats(), AeronStats(0, 0, 0, 0, 0, 0, 0, 0))

    def test_native_library_errors_and_discovery(self) -> None:
        with mock.patch("epoch.aeron_transport._find_native_library", return_value=None):
            with self.assertRaisesRegex(RuntimeError, "not found"):
                _NativeAeron()

        library = os.environ.get("EPOCH_AERON_LIBRARY")
        if not library:
            candidates = []
            current = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
            for _ in range(6):
                candidates.append(os.path.join(current, "native", "build", "libepoch_aeron.so"))
                current = os.path.dirname(current)
            library = next((path for path in candidates if os.path.exists(path)), None)
        if not library:
            self.skipTest("native epoch_aeron library is not built")

        native = _NativeAeron(library)
        with self.assertRaises(RuntimeError):
            native.open(AeronConfig("aeron:invalid-no-endpoint", 1, "/tmp/epoch-unusable"))

    def test_find_native_library_discovery_paths(self) -> None:
        # EPOCH_AERON_LIBRARY environment variable takes precedence.
        with mock.patch.dict(os.environ, {"EPOCH_AERON_LIBRARY": "/custom/libepoch.so"}):
            self.assertEqual(_find_native_library(), "/custom/libepoch.so")

        import ctypes.util

        # Linux: local build absent, system find_library returns a path.
        with mock.patch("epoch.aeron_transport.sys.platform", "linux"), \
             mock.patch.object(ctypes.util, "find_library", return_value="/usr/lib/libepoch_aeron.so"), \
             mock.patch("os.path.exists", return_value=False):
            self.assertEqual(_find_native_library(), "/usr/lib/libepoch_aeron.so")

        # Linux: find_library returns None -> None.
        with mock.patch("epoch.aeron_transport.sys.platform", "linux"), \
             mock.patch.object(ctypes.util, "find_library", return_value=None), \
             mock.patch("os.path.exists", return_value=False):
            self.assertIsNone(_find_native_library())

        # find_library raises -> fall back to None.
        with mock.patch("epoch.aeron_transport.sys.platform", "linux"), \
             mock.patch.object(ctypes.util, "find_library", side_effect=OSError("no lib")), \
             mock.patch("os.path.exists", return_value=False):
            self.assertIsNone(_find_native_library())

    def test_native_adapter_success_and_failure_paths(self) -> None:
        library = os.environ.get("EPOCH_AERON_LIBRARY")
        if not library:
            candidates = []
            current = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
            for _ in range(6):
                candidates.append(os.path.join(current, "native", "build", "libepoch_aeron.so"))
                current = os.path.dirname(current)
            library = next((path for path in candidates if os.path.exists(path)), None)
        if not library:
            self.skipTest("native epoch_aeron library is not built")

        native = _NativeAeron(library)
        self.assertEqual(native.poll(None, 0), [])

        import ctypes.util
        with mock.patch("epoch.aeron_transport.sys.platform", "darwin"), \
             mock.patch.object(ctypes.util, "find_library", return_value="/tmp/found.dylib"), \
             mock.patch("os.path.exists", return_value=False):
            self.assertEqual(_find_native_library(), "/tmp/found.dylib")

        with mock.patch("epoch.aeron_transport.sys.platform", "win32"), \
             mock.patch.object(ctypes.util, "find_library", return_value=None), \
             mock.patch("os.path.exists", return_value=False):
            self.assertIsNone(_find_native_library())

        with mock.patch.object(native, "_lib") as fake_lib:
            fake_lib.epoch_aeron_open.return_value = 0
            with self.assertRaises(RuntimeError):
                native.open(AeronConfig("aeron:ipc", 1, ""))

            fake_lib.epoch_aeron_open.return_value = 0x1234
            handle = native.open(AeronConfig("aeron:ipc", 1, ""))
            fake_lib.epoch_aeron_send.return_value = -1
            with self.assertRaises(RuntimeError):
                native.send(handle, b"\0" * 56)
            fake_lib.epoch_aeron_send.return_value = 0
            native.send(handle, b"\0" * 56)

            fake_lib.epoch_aeron_poll.return_value = -1
            with self.assertRaises(RuntimeError):
                native.poll(handle, 1)
            fake_lib.epoch_aeron_poll.return_value = 0
            def fill_poll(_handle, buffer, _max, out_count_ref, _err, _size):
                for index in range(56):
                    buffer[index] = index & 0xFF
                out_count_ref._obj.value = 1
                return 1
            fake_lib.epoch_aeron_poll.side_effect = fill_poll
            self.assertEqual(len(native.poll(handle, 1)), 1)
            fake_lib.epoch_aeron_poll.side_effect = None
            fake_lib.epoch_aeron_poll.return_value = 0

            fake_lib.epoch_aeron_stats.return_value = -1
            self.assertEqual(native.stats(handle), AeronStats(0, 0, 0, 0, 0, 0, 0, 0))
            fake_lib.epoch_aeron_stats.return_value = 0
            self.assertEqual(native.stats(handle).sent_count, 0)
            native.close(handle)
            native.close(None)

    def test_empty_poll_and_closed_native_handle(self) -> None:
        native = FakeNative()
        transport = AeronTransport(AeronConfig("aeron:ipc", 3, ""), native=native)
        self.assertEqual(native.poll(transport._handle, 0), [])
        transport.close()
        native.close(None)


if __name__ == "__main__":
    unittest.main()
