package io.epoch;

import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;

public final class VectorTestMain {
    public static void main(String[] args) throws Exception {
        Path vector = locateVectorFile();
        List<Engine.Message> messages = new ArrayList<>();
        List<Expected> expected = new ArrayList<>();

        for (String line : Files.readAllLines(vector)) {
            if (line.isBlank() || line.startsWith("#")) {
                continue;
            }
            String[] parts = line.split(",");
            if (parts.length == 0) {
                continue;
            }
            if ("M".equals(parts[0]) && parts.length == 7) {
                messages.add(new Engine.Message(
                    Long.parseLong(parts[1]),
                    Long.parseLong(parts[2]),
                    Long.parseLong(parts[3]),
                    Long.parseLong(parts[4]),
                    Long.parseLong(parts[5]),
                    Long.parseLong(parts[6])
                ));
            } else if ("E".equals(parts[0]) && parts.length == 4) {
                expected.add(new Expected(
                    Long.parseLong(parts[1]),
                    Long.parseLong(parts[2]),
                    parts[3]
                ));
            }
        }

        List<Engine.EpochResult> results = Engine.processMessages(messages);
        if (results.size() != expected.size()) {
            throw new IllegalStateException("Result count mismatch");
        }
        for (int i = 0; i < expected.size(); i++) {
            Expected exp = expected.get(i);
            Engine.EpochResult res = results.get(i);
            if (exp.epoch != res.epoch || exp.state != res.state || !exp.hash.equals(res.hash)) {
                throw new IllegalStateException("Vector mismatch at index " + i);
            }
        }
    }

    private static Path locateVectorFile() {
        Path current = Path.of(".").toAbsolutePath();
        for (int i = 0; i < 6; i++) {
            Path candidate = current.resolve("test-vectors").resolve("epoch_vector_v1.txt");
            if (Files.exists(candidate)) {
                return candidate;
            }
            current = current.getParent();
            if (current == null) {
                break;
            }
        }
        throw new IllegalStateException("Vector file not found");
    }

    private record Expected(long epoch, long state, String hash) {
    }
}
