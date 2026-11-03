package io.epoch;

import io.aeron.Aeron;
import io.aeron.Publication;
import io.aeron.Subscription;
import io.aeron.driver.MediaDriver;
import io.aeron.logbuffer.Header;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicLong;
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
        IdleStrategy idleStrategy,
        boolean embeddedDriver,
        boolean dirDeleteOnStart,
        boolean dirDeleteOnShutdown
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
            return new AeronConfig(channel, streamId, aeronDirectory, 64, 10, new BusySpinIdleStrategy(), false, false, false);
        }

        public static AeronConfig embeddedDefaults(String channel, int streamId, String aeronDirectory) {
            return new AeronConfig(channel, streamId, aeronDirectory, 64, 10, new BusySpinIdleStrategy(), true, true, true);
        }
    }

    interface AeronAdapter {
        long offer(DirectBuffer buffer, int offset, int length);

        int poll(io.aeron.logbuffer.FragmentHandler handler, int fragmentLimit);

        void close();
    }

    private static final class RealAeronAdapter implements AeronAdapter {
        private final Aeron aeron;
        private final Publication publication;
        private final Subscription subscription;
        private final MediaDriver mediaDriver;

        private RealAeronAdapter(AeronConfig config) {
            MediaDriver driver = null;
            String aeronDir = config.aeronDirectory();
            if (config.embeddedDriver()) {
                MediaDriver.Context driverContext = new MediaDriver.Context();
                if (aeronDir != null && !aeronDir.isBlank()) {
                    driverContext.aeronDirectoryName(aeronDir);
                }
                driverContext.dirDeleteOnStart(config.dirDeleteOnStart());
                driverContext.dirDeleteOnShutdown(config.dirDeleteOnShutdown());
                driver = MediaDriver.launch(driverContext);
                aeronDir = driverContext.aeronDirectoryName();
            }

            Aeron.Context context = new Aeron.Context();
            if (aeronDir != null && !aeronDir.isBlank()) {
                context.aeronDirectoryName(aeronDir);
            }
            this.aeron = Aeron.connect(context);
            this.publication = aeron.addPublication(config.channel(), config.streamId());
            this.subscription = aeron.addSubscription(config.channel(), config.streamId());
            this.mediaDriver = driver;
        }

        @Override
        public long offer(DirectBuffer buffer, int offset, int length) {
            return publication.offer(buffer, offset, length);
        }

        @Override
        public int poll(io.aeron.logbuffer.FragmentHandler handler, int fragmentLimit) {
            return subscription.poll(handler, fragmentLimit);
        }

        @Override
        public void close() {
            publication.close();
            subscription.close();
            aeron.close();
            if (mediaDriver != null) {
                mediaDriver.close();
            }
        }
    }

    private final AeronConfig config;
    private final AeronAdapter adapter;
    private final UnsafeBuffer sendBuffer;
    private final IdleStrategy idleStrategy;
    private final AeronStats stats = new AeronStats();
    private final Object closeLock = new Object();
    private volatile boolean closed;
    private volatile boolean released;

    public AeronTransport(AeronConfig config) {
        this(config, new RealAeronAdapter(config));
    }

    AeronTransport(AeronConfig config, AeronAdapter adapter) {
        this.config = config;
        this.adapter = adapter;
        this.sendBuffer = new UnsafeBuffer(new ExpandableArrayBuffer(AeronMessageCodec.FRAME_LENGTH));
        this.idleStrategy = config.idleStrategy();
    }

    @Override
    public void send(Engine.Message message) {
        synchronized (closeLock) {
            if (closed) {
                throw new IllegalStateException("Aeron transport is closed");
            }
            AeronMessageCodec.encode(sendBuffer, 0, message);
            int attempts = 0;
            long result;
            do {
                result = adapter.offer(sendBuffer, 0, AeronMessageCodec.FRAME_LENGTH);
                if (result >= 0) {
                    stats.recordSent();
                    return;
                }
                stats.recordOfferFailure(result);
                attempts++;
                idleStrategy.idle();
            } while (attempts < config.offerMaxAttempts());

            throw new IllegalStateException("Aeron offer failed: " + result);
        }
    }

    @Override
    public List<Engine.Message> poll(int max) {
        synchronized (closeLock) {
            if (closed || max <= 0) {
                return List.of();
            }
            int limit = Math.min(max, config.fragmentLimit());
            List<Engine.Message> out = new ArrayList<>();
            adapter.poll((DirectBuffer buffer, int offset, int length, Header header) -> {
                if (length < AeronMessageCodec.FRAME_LENGTH) {
                    return;
                }
                out.add(AeronMessageCodec.decode(buffer, offset));
                stats.recordReceived();
            }, limit);
            return out;
        }
    }

    @Override
    public void close() {
        synchronized (closeLock) {
            if (closed && released) {
                return;
            }
            closed = true;
            adapter.close();
            released = true;
        }
    }

    public AeronConfig config() {
        return config;
    }

    public AeronStats stats() {
        return stats;
    }

    public static final class AeronStats {
        private final AtomicLong sentCount = new AtomicLong();
        private final AtomicLong receivedCount = new AtomicLong();
        private final AtomicLong offerBackPressure = new AtomicLong();
        private final AtomicLong offerNotConnected = new AtomicLong();
        private final AtomicLong offerAdminAction = new AtomicLong();
        private final AtomicLong offerClosed = new AtomicLong();
        private final AtomicLong offerFailed = new AtomicLong();

        private void recordOfferFailure(long result) {
            if (result == Publication.BACK_PRESSURED) {
                offerBackPressure.incrementAndGet();
            } else if (result == Publication.NOT_CONNECTED) {
                offerNotConnected.incrementAndGet();
            } else if (result == Publication.ADMIN_ACTION) {
                offerAdminAction.incrementAndGet();
            } else if (result == Publication.CLOSED) {
                offerClosed.incrementAndGet();
            } else {
                offerFailed.incrementAndGet();
            }
        }

        private void recordSent() {
            sentCount.incrementAndGet();
        }

        private void recordReceived() {
            receivedCount.incrementAndGet();
        }

        public long sentCount() {
            return sentCount.get();
        }

        public long receivedCount() {
            return receivedCount.get();
        }

        public long offerBackPressure() {
            return offerBackPressure.get();
        }

        public long offerNotConnected() {
            return offerNotConnected.get();
        }

        public long offerAdminAction() {
            return offerAdminAction.get();
        }

        public long offerClosed() {
            return offerClosed.get();
        }

        public long offerFailed() {
            return offerFailed.get();
        }
    }
}
