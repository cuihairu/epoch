package io.epoch;

import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;

public final class VectorTestMain {
    public static void main(String[] args) throws Exception {
        ensure("0.1.0".equals(Version.value()), "Version mismatch");
        ensure("c3c43df01be7b59c".equals(Engine.fnv1a64Hex("state:0")), "Hash mismatch");
        ensure("a430d84680aabd0b".equals(Engine.fnv1a64Hex("hello")), "Hash mismatch");
        ensure(Engine.processMessages(new ArrayList<>()).isEmpty(), "Empty process failed");
        ActorId.Parts actorParts = new ActorId.Parts(1, 2, 3, 4, 5);
        long actorId = ActorId.encode(actorParts);
        ActorId.Parts decoded = ActorId.decode(actorId);
        ensure(actorParts.equals(decoded), "ActorId codec mismatch");

        List<Engine.Message> sampleMessages = new ArrayList<>();
        sampleMessages.add(new Engine.Message(2, 2, 1, 2, 100, 0, 5));
        sampleMessages.add(new Engine.Message(1, 1, 2, 1, 100, 0, 2));
        sampleMessages.add(new Engine.Message(1, 1, 1, 2, 100, 0, -1));
        sampleMessages.add(new Engine.Message(3, 1, 1, 1, 100, 0, 4));
        List<Engine.EpochResult> sampleResults = Engine.processMessages(sampleMessages);
        ensure(sampleResults.size() == 3, "Sample result count mismatch");
        ensure(sampleResults.get(0).epoch == 1 && sampleResults.get(0).state == 1 &&
            "c3c43ef01be7b74f".equals(sampleResults.get(0).hash), "Sample epoch 1 mismatch");
        ensure(sampleResults.get(1).epoch == 2 && sampleResults.get(1).state == 6 &&
            "c3c43bf01be7b236".equals(sampleResults.get(1).hash), "Sample epoch 2 mismatch");
        ensure(sampleResults.get(2).epoch == 3 && sampleResults.get(2).state == 10 &&
            "8e2e70ff6abccccd".equals(sampleResults.get(2).hash), "Sample epoch 3 mismatch");

        InMemoryTransport transport = new InMemoryTransport();
        transport.send(new Engine.Message(1, 1, 1, 1, 100, 0, 1));
        transport.send(new Engine.Message(1, 1, 1, 2, 100, 0, 2));
        transport.send(new Engine.Message(2, 1, 1, 3, 100, 0, 3));
        ensure(transport.poll(0).isEmpty(), "Transport poll(0) should be empty");
        List<Engine.Message> first = transport.poll(2);
        ensure(first.size() == 2 && first.get(0).payload == 1 && first.get(1).payload == 2,
            "Transport first poll mismatch");
        List<Engine.Message> second = transport.poll(5);
        ensure(second.size() == 1 && second.get(0).payload == 3, "Transport second poll mismatch");
        transport.close();
        ensure(transport.poll(1).isEmpty(), "Transport close mismatch");

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
            if ("M".equals(parts[0]) && (parts.length == 7 || parts.length == 8)) {
                int qos = parts.length == 8 ? Integer.parseInt(parts[6]) : 0;
                long payload = parts.length == 8 ? Long.parseLong(parts[7]) : Long.parseLong(parts[6]);
                messages.add(new Engine.Message(
                    Long.parseLong(parts[1]),
                    Long.parseLong(parts[2]),
                    Long.parseLong(parts[3]),
                    Long.parseLong(parts[4]),
                    Long.parseLong(parts[5]),
                    qos,
                    payload
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

    private static void ensure(boolean condition, String message) {
        if (!condition) {
            throw new IllegalStateException(message);
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
