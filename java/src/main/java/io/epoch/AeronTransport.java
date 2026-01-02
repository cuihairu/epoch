package io.epoch;

import io.aeron.Aeron;
import io.aeron.Publication;
import io.aeron.Subscription;
import io.aeron.logbuffer.Header;
import java.util.ArrayList;
import java.util.List;
import org.agrona.concurrent.BusySpinIdleStrategy;
import org.agrona.concurrent.IdleStrategy;
import org.agrona.DirectBuffer;
import org.agrona.ExpandableArrayBuffer;
import org.agrona.concurrent.UnsafeBuffer;

public final class AeronTransport implements Transport {
    public record AeronConfig(
        String channel,
        int streamId,
        String aeronDirectory,
        int fragmentLimit,
        int offerMaxAttempts,
        IdleStrategy idleStrategy
    ) {
        public AeronConfig {
            if (fragmentLimit <= 0) {
                fragmentLimit = 64;
            }
            if (offerMaxAttempts <= 0) {
                offerMaxAttempts = 10;
            }
            if (idleStrategy == null) {
                idleStrategy = new BusySpinIdleStrategy();
            }
        }

        public static AeronConfig defaults(String channel, int streamId, String aeronDirectory) {
            return new AeronConfig(channel, streamId, aeronDirectory, 64, 10, new BusySpinIdleStrategy());
        }
    }

    private final AeronConfig config;
    private final Aeron aeron;
    private final Publication publication;
    private final Subscription subscription;
    private final UnsafeBuffer sendBuffer;
    private final IdleStrategy idleStrategy;
    private final AeronStats stats = new AeronStats();

    public AeronTransport(AeronConfig config) {
        this.config = config;
        Aeron.Context context = new Aeron.Context();
        if (config.aeronDirectory() != null && !config.aeronDirectory().isBlank()) {
            context.aeronDirectoryName(config.aeronDirectory());
        }
        this.aeron = Aeron.connect(context);
        this.publication = aeron.addPublication(config.channel(), config.streamId());
        this.subscription = aeron.addSubscription(config.channel(), config.streamId());
        this.sendBuffer = new UnsafeBuffer(new ExpandableArrayBuffer(AeronMessageCodec.FRAME_LENGTH));
        this.idleStrategy = config.idleStrategy();
    }

    @Override
    public void send(Engine.Message message) {
        AeronMessageCodec.encode(sendBuffer, 0, message);
        int attempts = 0;
        long result;
        do {
            result = publication.offer(sendBuffer, 0, AeronMessageCodec.FRAME_LENGTH);
            if (result >= 0) {
                stats.sentCount++;
                return;
            }
            stats.recordOfferFailure(result);
            attempts++;
            idleStrategy.idle();
        } while (attempts < config.offerMaxAttempts());

        throw new IllegalStateException("Aeron offer failed: " + result);
    }

    @Override
    public List<Engine.Message> poll(int max) {
        if (max <= 0) {
            return List.of();
        }
        int limit = Math.min(max, config.fragmentLimit());
        List<Engine.Message> out = new ArrayList<>();
        subscription.poll((DirectBuffer buffer, int offset, int length, Header header) -> {
            if (length < AeronMessageCodec.FRAME_LENGTH) {
                return;
            }
            out.add(AeronMessageCodec.decode(buffer, offset));
            stats.receivedCount++;
        }, limit);
        return out;
    }

    @Override
    public void close() {
        publication.close();
        subscription.close();
        aeron.close();
    }

    public AeronConfig config() {
        return config;
    }

    public AeronStats stats() {
        return stats;
    }

    public static final class AeronStats {
        private long sentCount;
        private long receivedCount;
        private long offerBackPressure;
        private long offerNotConnected;
        private long offerAdminAction;
        private long offerClosed;
        private long offerFailed;

        private void recordOfferFailure(long result) {
            if (result == Publication.BACK_PRESSURED) {
                offerBackPressure++;
            } else if (result == Publication.NOT_CONNECTED) {
                offerNotConnected++;
            } else if (result == Publication.ADMIN_ACTION) {
                offerAdminAction++;
            } else if (result == Publication.CLOSED) {
                offerClosed++;
            } else {
                offerFailed++;
            }
        }

        public long sentCount() {
            return sentCount;
        }

        public long receivedCount() {
            return receivedCount;
        }

        public long offerBackPressure() {
            return offerBackPressure;
        }

        public long offerNotConnected() {
            return offerNotConnected;
        }

        public long offerAdminAction() {
            return offerAdminAction;
        }

        public long offerClosed() {
            return offerClosed;
        }

        public long offerFailed() {
            return offerFailed;
        }
    }
}
