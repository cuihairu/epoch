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
