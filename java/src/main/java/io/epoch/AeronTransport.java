package io.epoch;

import io.aeron.Aeron;
import io.aeron.Publication;
import io.aeron.Subscription;
import io.aeron.logbuffer.Header;
import java.util.ArrayList;
import java.util.List;
import org.agrona.DirectBuffer;
import org.agrona.ExpandableArrayBuffer;
import org.agrona.concurrent.UnsafeBuffer;

public final class AeronTransport implements Transport {
    public record AeronConfig(String channel, int streamId, String aeronDirectory) {
    }

    private final AeronConfig config;
    private final Aeron aeron;
    private final Publication publication;
    private final Subscription subscription;
    private final UnsafeBuffer sendBuffer;
    private final int fragmentLimit = 64;

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
    }

    @Override
    public void send(Engine.Message message) {
        AeronMessageCodec.encode(sendBuffer, 0, message);
        int attempts = 0;
        long result;
        do {
            result = publication.offer(sendBuffer, 0, AeronMessageCodec.FRAME_LENGTH);
            if (result >= 0) {
                return;
            }
            attempts++;
            Thread.onSpinWait();
        } while (attempts < 10);

        throw new IllegalStateException("Aeron offer failed: " + result);
    }

    @Override
    public List<Engine.Message> poll(int max) {
        if (max <= 0) {
            return List.of();
        }
        int limit = Math.min(max, fragmentLimit);
        List<Engine.Message> out = new ArrayList<>();
        subscription.poll((DirectBuffer buffer, int offset, int length, Header header) -> {
            if (length < AeronMessageCodec.FRAME_LENGTH) {
                return;
            }
            out.add(AeronMessageCodec.decode(buffer, offset));
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
}
