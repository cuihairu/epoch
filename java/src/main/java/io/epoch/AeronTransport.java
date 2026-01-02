package io.epoch;

import java.util.List;

public final class AeronTransport implements Transport {
    public record AeronConfig(String channel, int streamId, String aeronDirectory) {
    }

    private final AeronConfig config;

    public AeronTransport(AeronConfig config) {
        this.config = config;
    }

    @Override
    public void send(Engine.Message message) {
        throw new UnsupportedOperationException("Aeron transport not linked");
    }

    @Override
    public List<Engine.Message> poll(int max) {
        throw new UnsupportedOperationException("Aeron transport not linked");
    }

    @Override
    public void close() {
        // no-op for stub
    }

    public AeronConfig config() {
        return config;
    }
}
