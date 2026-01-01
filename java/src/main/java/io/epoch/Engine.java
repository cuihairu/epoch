package io.epoch;

import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;

public final class Engine {
    private static final long FNV_OFFSET_BASIS = 0xcbf29ce484222325L;
    private static final long FNV_PRIME = 0x100000001b3L;

    private Engine() {
    }

    public static final class Message {
        public final long epoch;
        public final long channelId;
        public final long sourceId;
        public final long sourceSeq;
        public final long schemaId;
        public final long payload;

        public Message(long epoch, long channelId, long sourceId, long sourceSeq, long schemaId, long payload) {
            this.epoch = epoch;
            this.channelId = channelId;
            this.sourceId = sourceId;
            this.sourceSeq = sourceSeq;
            this.schemaId = schemaId;
            this.payload = payload;
        }
    }

    public static final class EpochResult {
        public final long epoch;
        public final long state;
        public final String hash;

        public EpochResult(long epoch, long state, String hash) {
            this.epoch = epoch;
            this.state = state;
            this.hash = hash;
        }
    }

    public static String fnv1a64Hex(String input) {
        long hash = FNV_OFFSET_BASIS;
        byte[] bytes = input.getBytes(java.nio.charset.StandardCharsets.UTF_8);
        for (byte b : bytes) {
            hash ^= (b & 0xffL);
            hash *= FNV_PRIME;
        }
        return toHex(hash);
    }

    public static List<EpochResult> processMessages(List<Message> messages) {
        messages.sort(Comparator
            .comparingLong((Message m) -> m.epoch)
            .thenComparingLong(m -> m.channelId)
            .thenComparingLong(m -> m.sourceId)
            .thenComparingLong(m -> m.sourceSeq));

        List<EpochResult> results = new ArrayList<>();
        long state = 0;
        long currentEpoch = 0;
        boolean hasEpoch = false;

        for (Message message : messages) {
            if (!hasEpoch) {
                currentEpoch = message.epoch;
                hasEpoch = true;
            }
            if (message.epoch != currentEpoch) {
                results.add(new EpochResult(currentEpoch, state, fnv1a64Hex("state:" + state)));
                currentEpoch = message.epoch;
            }
            state += message.payload;
        }

        if (hasEpoch) {
            results.add(new EpochResult(currentEpoch, state, fnv1a64Hex("state:" + state)));
        }

        return results;
    }

    private static String toHex(long value) {
        String hex = Long.toUnsignedString(value, 16);
        if (hex.length() >= 16) {
            return hex;
        }
        StringBuilder sb = new StringBuilder(16);
        for (int i = hex.length(); i < 16; i++) {
            sb.append('0');
        }
        sb.append(hex);
        return sb.toString();
    }
}
