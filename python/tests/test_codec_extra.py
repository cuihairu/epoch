import ctypes
import os
import unittest
from unittest import mock

from epoch.actor_id import default_actor_id_codec
from epoch.aeron_transport import _find_native_library


class CodecNameTest(unittest.TestCase):
    def test_default_codec_name(self) -> None:
        self.assertEqual(default_actor_id_codec.name, "default")


class NativeLibraryDiscoveryTest(unittest.TestCase):
    def test_discovery_finds_local_build(self) -> None:
        repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
        candidate = os.path.join(repo_root, "native", "build", "libepoch_aeron.so")
        if not os.path.exists(candidate):
            self.skipTest("native build not present")
        env = {k: v for k, v in os.environ.items() if k != "EPOCH_AERON_LIBRARY"}
        with mock.patch.dict(os.environ, env, clear=True):
            found = _find_native_library()
        self.assertIsNotNone(found)
        self.assertTrue(found.endswith(os.path.join("native", "build", "libepoch_aeron.so")))

    def test_discovery_breaks_at_root_without_library(self) -> None:
        with mock.patch("epoch.aeron_transport.__file__", "/x/y/aeron_transport.py"), \
             mock.patch("epoch.aeron_transport.sys.platform", "linux"), \
             mock.patch("os.path.exists", return_value=False), \
             mock.patch.object(ctypes.util, "find_library", return_value=None):
            self.assertIsNone(_find_native_library())


if __name__ == "__main__":
    unittest.main()
