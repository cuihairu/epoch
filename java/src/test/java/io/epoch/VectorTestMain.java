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
        ensure("default".equals(ActorId.DEFAULT.name()), "ActorId codec name mismatch");
        boolean actorIdFailed = false;
        try {
            ActorId.encode(new ActorId.Parts(1 << 10, 1, 1, 1, 1));
        } catch (IllegalArgumentException ex) {
            actorIdFailed = true;
        }
        ensure(actorIdFailed, "ActorId out of range mismatch");

        AeronTransport.AeronConfig normalized = new AeronTransport.AeronConfig(
            "aeron:ipc", 1, "", 0, 0, null, false, false, false);
        ensure(normalized.fragmentLimit() == 64, "AeronConfig fragment limit mismatch");
        ensure(normalized.offerMaxAttempts() == 10, "AeronConfig offer attempts mismatch");
        ensure(normalized.idleStrategy() != null, "AeronConfig idle strategy mismatch");

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

        org.agrona.concurrent.UnsafeBuffer buffer =
            new org.agrona.concurrent.UnsafeBuffer(new org.agrona.ExpandableArrayBuffer(AeronMessageCodec.FRAME_LENGTH));
        Engine.Message codecMessage = new Engine.Message(9, 8, 7, 6, 5, 4, -3);
        AeronMessageCodec.encode(buffer, 0, codecMessage);
        Engine.Message decodedMessage = AeronMessageCodec.decode(buffer, 0);
        ensure(decodedMessage.epoch == codecMessage.epoch &&
               decodedMessage.channelId == codecMessage.channelId &&
               decodedMessage.sourceId == codecMessage.sourceId &&
               decodedMessage.sourceSeq == codecMessage.sourceSeq &&
               decodedMessage.schemaId == codecMessage.schemaId &&
               decodedMessage.qos == codecMessage.qos &&
               decodedMessage.payload == codecMessage.payload, "AeronMessageCodec mismatch");
        buffer.putByte(0, (byte) 2);
        boolean codecFailed = false;
        try {
            AeronMessageCodec.decode(buffer, 0);
        } catch (IllegalArgumentException ex) {
            codecFailed = true;
        }
        ensure(codecFailed, "AeronMessageCodec version mismatch");

        AeronTransport.AeronConfig aeronConfig = new AeronTransport.AeronConfig(
            "aeron:ipc", 42, "", 64, 10, new org.agrona.concurrent.BusySpinIdleStrategy(), false, false, false);
        AeronTransport aeronTransport = new AeronTransport(aeronConfig, new FakeAeronAdapter());
        try {
            List<Engine.Message> aeronOut = List.of();
            Engine.Message aeronMessage = new Engine.Message(1, 1, 1, 1, 1, 1, 5);
            aeronTransport.send(aeronMessage);
            aeronOut = aeronTransport.poll(1);
            ensure(!aeronOut.isEmpty(), "AeronTransport poll empty");
            ensure(aeronOut.get(0).payload == 5, "AeronTransport payload mismatch");
            ensure(aeronTransport.stats().sentCount() >= 1, "AeronTransport stats sent mismatch");
            ensure(aeronTransport.stats().receivedCount() >= 1, "AeronTransport stats received mismatch");
            ensure(aeronTransport.poll(0).isEmpty(), "AeronTransport poll(0) mismatch");
        } finally {
            aeronTransport.close();
        }

        AeronTransport.AeronConfig failingConfig = new AeronTransport.AeronConfig(
            "aeron:ipc", 42, "", 64, 2, new org.agrona.concurrent.BusySpinIdleStrategy(), false, false, false);
        AeronTransport failingTransport = new AeronTransport(
            failingConfig,
            new FakeAeronAdapter(io.aeron.Publication.BACK_PRESSURED, io.aeron.Publication.BACK_PRESSURED));
        try {
            boolean failed = false;
            try {
                failingTransport.send(new Engine.Message(1, 1, 1, 1, 1, 1, 1));
            } catch (IllegalStateException ex) {
                failed = true;
            }
            ensure(failed, "AeronTransport offer failure mismatch");
            ensure(failingTransport.stats().offerBackPressure() == 2, "AeronTransport stats back pressure mismatch");
        } finally {
            failingTransport.close();
        }

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

    private static final class FakeAeronAdapter implements AeronTransport.AeronAdapter {
        private final List<byte[]> frames = new ArrayList<>();
        private final List<Long> offerResults = new ArrayList<>();

        FakeAeronAdapter(long... results) {
            for (long result : results) {
                offerResults.add(result);
            }
        }

        @Override
        public long offer(org.agrona.DirectBuffer buffer, int offset, int length) {
            if (!offerResults.isEmpty()) {
                long result = offerResults.remove(0);
                if (result < 0) {
                    return result;
                }
            }
            byte[] copy = new byte[length];
            buffer.getBytes(offset, copy);
            frames.add(copy);
            return 1;
        }

        @Override
        public int poll(io.aeron.logbuffer.FragmentHandler handler, int fragmentLimit) {
            int count = 0;
            while (count < fragmentLimit && !frames.isEmpty()) {
                byte[] frame = frames.remove(0);
                handler.onFragment(new org.agrona.concurrent.UnsafeBuffer(frame), 0, frame.length, null);
                count++;
            }
            return count;
        }

        @Override
        public void close() {
            frames.clear();
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
