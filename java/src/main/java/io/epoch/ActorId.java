package io.epoch;

public final class ActorId {
    private ActorId() {
    }

    public record Parts(int region, int server, int processType, int processIndex, int actorIndex) {
    }

    public interface Codec {
        long encode(Parts parts);

        Parts decode(long value);

        String name();
    }

    public static final Codec DEFAULT = new DefaultCodec();

    public static long encode(Parts parts) {
        return DEFAULT.encode(parts);
    }

    public static Parts decode(long value) {
        return DEFAULT.decode(value);
    }

    private static final class DefaultCodec implements Codec {
        private static final int REGION_BITS = 10;
        private static final int SERVER_BITS = 12;
        private static final int PROCESS_TYPE_BITS = 6;
        private static final int PROCESS_INDEX_BITS = 10;
        private static final int ACTOR_INDEX_BITS = 26;

        private static final long REGION_MAX = (1L << REGION_BITS) - 1;
        private static final long SERVER_MAX = (1L << SERVER_BITS) - 1;
        private static final long PROCESS_TYPE_MAX = (1L << PROCESS_TYPE_BITS) - 1;
        private static final long PROCESS_INDEX_MAX = (1L << PROCESS_INDEX_BITS) - 1;
        private static final long ACTOR_INDEX_MAX = (1L << ACTOR_INDEX_BITS) - 1;

        private static final int ACTOR_INDEX_SHIFT = 0;
        private static final int PROCESS_INDEX_SHIFT = ACTOR_INDEX_SHIFT + ACTOR_INDEX_BITS;
        private static final int PROCESS_TYPE_SHIFT = PROCESS_INDEX_SHIFT + PROCESS_INDEX_BITS;
        private static final int SERVER_SHIFT = PROCESS_TYPE_SHIFT + PROCESS_TYPE_BITS;
        private static final int REGION_SHIFT = SERVER_SHIFT + SERVER_BITS;

        @Override
        public long encode(Parts parts) {
            if (parts.region < 0 || parts.region > REGION_MAX ||
                parts.server < 0 || parts.server > SERVER_MAX ||
                parts.processType < 0 || parts.processType > PROCESS_TYPE_MAX ||
                parts.processIndex < 0 || parts.processIndex > PROCESS_INDEX_MAX ||
                parts.actorIndex < 0 || parts.actorIndex > ACTOR_INDEX_MAX) {
                throw new IllegalArgumentException("ActorId parts out of range");
            }
            return (long) parts.region << REGION_SHIFT
                | (long) parts.server << SERVER_SHIFT
                | (long) parts.processType << PROCESS_TYPE_SHIFT
                | (long) parts.processIndex << PROCESS_INDEX_SHIFT
                | (long) parts.actorIndex << ACTOR_INDEX_SHIFT;
        }

        @Override
        public Parts decode(long value) {
            int region = (int) ((value >> REGION_SHIFT) & REGION_MAX);
            int server = (int) ((value >> SERVER_SHIFT) & SERVER_MAX);
            int processType = (int) ((value >> PROCESS_TYPE_SHIFT) & PROCESS_TYPE_MAX);
            int processIndex = (int) ((value >> PROCESS_INDEX_SHIFT) & PROCESS_INDEX_MAX);
            int actorIndex = (int) ((value >> ACTOR_INDEX_SHIFT) & ACTOR_INDEX_MAX);
            return new Parts(region, server, processType, processIndex, actorIndex);
        }

        @Override
        public String name() {
            return "default";
        }
    }
}
