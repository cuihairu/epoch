package io.epoch;

import org.agrona.DirectBuffer;
import org.agrona.concurrent.UnsafeBuffer;

public final class AeronMessageCodec {
    public static final int VERSION = 1;
    public static final int FRAME_LENGTH = 56;

    private static final int OFFSET_VERSION = 0;
    private static final int OFFSET_QOS = 1;
    private static final int OFFSET_EPOCH = 8;
    private static final int OFFSET_CHANNEL = 16;
    private static final int OFFSET_SOURCE = 24;
    private static final int OFFSET_SOURCE_SEQ = 32;
    private static final int OFFSET_SCHEMA = 40;
    private static final int OFFSET_PAYLOAD = 48;

    private AeronMessageCodec() {
    }

    public static void encode(UnsafeBuffer buffer, int offset, Engine.Message message) {
        buffer.putByte(offset + OFFSET_VERSION, (byte) VERSION);
        buffer.putByte(offset + OFFSET_QOS, (byte) message.qos);
        buffer.putLong(offset + OFFSET_EPOCH, message.epoch);
        buffer.putLong(offset + OFFSET_CHANNEL, message.channelId);
        buffer.putLong(offset + OFFSET_SOURCE, message.sourceId);
        buffer.putLong(offset + OFFSET_SOURCE_SEQ, message.sourceSeq);
        buffer.putLong(offset + OFFSET_SCHEMA, message.schemaId);
        buffer.putLong(offset + OFFSET_PAYLOAD, message.payload);
    }

    public static Engine.Message decode(DirectBuffer buffer, int offset) {
        int version = buffer.getByte(offset + OFFSET_VERSION);
        if (version != VERSION) {
            throw new IllegalArgumentException("Unsupported message version: " + version);
        }
        int qos = buffer.getByte(offset + OFFSET_QOS) & 0xff;
        long epoch = buffer.getLong(offset + OFFSET_EPOCH);
        long channelId = buffer.getLong(offset + OFFSET_CHANNEL);
        long sourceId = buffer.getLong(offset + OFFSET_SOURCE);
        long sourceSeq = buffer.getLong(offset + OFFSET_SOURCE_SEQ);
        long schemaId = buffer.getLong(offset + OFFSET_SCHEMA);
        long payload = buffer.getLong(offset + OFFSET_PAYLOAD);
        return new Engine.Message(epoch, channelId, sourceId, sourceSeq, schemaId, qos, payload);
    }
}
