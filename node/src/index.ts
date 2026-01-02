import { decodeActorId, defaultActorIdCodec, encodeActorId } from "./actor-id";
import { AeronNative, AeronStats, loadAeronNative } from "./aeron-native";

export type Message = {
  epoch: number;
  channelId: number;
  sourceId: number;
  sourceSeq: number;
  schemaId: number;
  qos?: number;
  payload: number;
};

export type AeronConfig = {
  channel: string;
  streamId: number;
  aeronDirectory: string;
  fragmentLimit?: number;
  offerMaxAttempts?: number;
};

export function version(): string {
  return "0.1.0";
}

const FNV_OFFSET_BASIS = 0xcbf29ce484222325n;
const FNV_PRIME = 0x100000001b3n;
const FNV_MASK = 0xffffffffffffffffn;

export function fnv1a64Hex(input: string): string {
  let hash = FNV_OFFSET_BASIS;
  const bytes = Buffer.from(input, "utf8");
  for (const b of bytes) {
    hash ^= BigInt(b);
    hash = (hash * FNV_PRIME) & FNV_MASK;
  }
  return hash.toString(16).padStart(16, "0");
}

export function processMessages(messages: Message[]) {
  messages.sort((a, b) => {
    if (a.epoch !== b.epoch) return a.epoch - b.epoch;
    if (a.channelId !== b.channelId) return a.channelId - b.channelId;
    const qosA = a.qos ?? 0;
    const qosB = b.qos ?? 0;
    if (qosA !== qosB) return qosB - qosA;
    if (a.sourceId !== b.sourceId) return a.sourceId - b.sourceId;
    return a.sourceSeq - b.sourceSeq;
  });

  const results: { epoch: number; state: number; hash: string }[] = [];
  let state = 0;
  let currentEpoch: number | null = null;

  for (const message of messages) {
    if (currentEpoch === null) {
      currentEpoch = message.epoch;
    }
    if (message.epoch !== currentEpoch) {
      results.push({
        epoch: currentEpoch,
        state,
        hash: fnv1a64Hex(`state:${state}`)
      });
      currentEpoch = message.epoch;
    }
    state += message.payload;
  }

  if (currentEpoch !== null) {
    results.push({
      epoch: currentEpoch,
      state,
      hash: fnv1a64Hex(`state:${state}`)
    });
  }

  return results;
}

const AERON_FRAME_LENGTH = 56;
const AERON_FRAME_VERSION = 1;

export function encodeAeronFrame(message: Message) {
  const buffer = Buffer.alloc(AERON_FRAME_LENGTH);
  buffer.writeUInt8(AERON_FRAME_VERSION, 0);
  buffer.writeUInt8(message.qos ?? 0, 1);
  buffer.writeBigInt64LE(BigInt(message.epoch), 8);
  buffer.writeBigInt64LE(BigInt(message.channelId), 16);
  buffer.writeBigInt64LE(BigInt(message.sourceId), 24);
  buffer.writeBigInt64LE(BigInt(message.sourceSeq), 32);
  buffer.writeBigInt64LE(BigInt(message.schemaId), 40);
  buffer.writeBigInt64LE(BigInt(message.payload), 48);
  return buffer;
}

export function decodeAeronFrame(buffer: Buffer): Message {
  if (buffer.length < AERON_FRAME_LENGTH) {
    throw new Error("Aeron frame too short");
  }
  if (buffer.readUInt8(0) !== AERON_FRAME_VERSION) {
    throw new Error("Unsupported Aeron frame version");
  }
  return {
    epoch: Number(buffer.readBigInt64LE(8)),
    channelId: Number(buffer.readBigInt64LE(16)),
    sourceId: Number(buffer.readBigInt64LE(24)),
    sourceSeq: Number(buffer.readBigInt64LE(32)),
    schemaId: Number(buffer.readBigInt64LE(40)),
    qos: buffer.readUInt8(1),
    payload: Number(buffer.readBigInt64LE(48))
  };
}

export class InMemoryTransport {
  private queue: Message[] = [];

  send(message: Message) {
    this.queue.push(message);
  }

  poll(max: number) {
    if (!max || this.queue.length === 0) {
      return [];
    }
    const count = Math.min(max, this.queue.length);
    return this.queue.splice(0, count);
  }

  close() {
    this.queue.length = 0;
  }
}

export class AeronTransport {
  private readonly native: AeronNative;
  private readonly handle: unknown;
  private closed = false;

  constructor(public readonly config: AeronConfig, native?: AeronNative) {
    this.native = native ?? loadAeronNative();
    this.handle = this.native.open(config);
  }

  send(message: Message) {
    if (this.closed) {
      throw new Error("Aeron transport closed");
    }
    const frame = encodeAeronFrame(message);
    this.native.send(this.handle, frame);
  }

  poll(max: number) {
    if (this.closed || max <= 0) {
      return [];
    }
    const frames = this.native.poll(this.handle, max);
    return frames.map(decodeAeronFrame);
  }

  stats(): AeronStats {
    if (this.closed) {
      return {
        sentCount: 0,
        receivedCount: 0,
        offerBackPressure: 0,
        offerNotConnected: 0,
        offerAdminAction: 0,
        offerClosed: 0,
        offerMaxPosition: 0,
        offerFailed: 0
      };
    }
    return this.native.stats(this.handle);
  }

  close() {
    if (this.closed) {
      return;
    }
    this.closed = true;
    this.native.close(this.handle);
  }
}

export { defaultActorIdCodec, encodeActorId, decodeActorId };
export type { AeronStats } from "./aeron-native";
