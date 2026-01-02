import os
import unittest

import epoch


class VectorTest(unittest.TestCase):
    def test_vector_matches_expected(self):
        vector_path = locate_vector_file()
        messages = []
        expected = []

        with open(vector_path, "r", encoding="utf-8") as handle:
            for line in handle:
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                parts = line.split(",")
                if parts[0] == "M" and len(parts) == 7:
                    messages.append(
                        epoch.Message(
                            epoch=int(parts[1]),
                            channel_id=int(parts[2]),
                            source_id=int(parts[3]),
                            source_seq=int(parts[4]),
                            schema_id=int(parts[5]),
                            payload=int(parts[6]),
                        )
                    )
                elif parts[0] == "E" and len(parts) == 4:
                    expected.append(
                        (int(parts[1]), int(parts[2]), parts[3])
                    )

        results = epoch.process_messages(messages)
        self.assertEqual(len(results), len(expected))
        for result, exp in zip(results, expected):
            self.assertEqual(result.epoch, exp[0])
            self.assertEqual(result.state, exp[1])
            self.assertEqual(result.hash, exp[2])

    def test_version(self):
        self.assertEqual(epoch.version(), "0.1.0")

    def test_fnv1a64_hex(self):
        self.assertEqual(epoch.fnv1a64_hex("state:0"), "c3c43df01be7b59c")
        self.assertEqual(epoch.fnv1a64_hex("hello"), "a430d84680aabd0b")

    def test_process_messages_empty(self):
        self.assertEqual(epoch.process_messages([]), [])

    def test_process_messages_ordering(self):
        messages = [
            epoch.Message(2, 2, 1, 2, 100, 5),
            epoch.Message(1, 1, 2, 1, 100, 2),
            epoch.Message(1, 1, 1, 2, 100, -1),
            epoch.Message(3, 1, 1, 1, 100, 4),
        ]
        results = epoch.process_messages(messages)
        self.assertEqual(len(results), 3)
        self.assertEqual(results[0], epoch.EpochResult(1, 1, "c3c43ef01be7b74f"))
        self.assertEqual(results[1], epoch.EpochResult(2, 6, "c3c43bf01be7b236"))
        self.assertEqual(results[2], epoch.EpochResult(3, 10, "8e2e70ff6abccccd"))

    def test_in_memory_transport(self):
        transport = epoch.InMemoryTransport()
        transport.send(epoch.Message(1, 1, 1, 1, 100, 1))
        transport.send(epoch.Message(1, 1, 1, 2, 100, 2))
        transport.send(epoch.Message(2, 1, 1, 3, 100, 3))
        self.assertEqual(transport.poll(0), [])
        first = transport.poll(2)
        self.assertEqual([m.payload for m in first], [1, 2])
        second = transport.poll(5)
        self.assertEqual([m.payload for m in second], [3])
        transport.close()
        self.assertEqual(transport.poll(1), [])


def locate_vector_file() -> str:
    current = os.path.abspath(".")
    for _ in range(6):
        candidate = os.path.join(current, "test-vectors", "epoch_vector_v1.txt")
        if os.path.exists(candidate):
            return candidate
        parent = os.path.dirname(current)
        if parent == current:
            break
        current = parent
    raise FileNotFoundError("Vector file not found")


if __name__ == "__main__":
    unittest.main()
