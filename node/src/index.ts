import { decodeActorId, defaultActorIdCodec, encodeActorId } from "./actor-id";

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
  constructor(public readonly config: AeronConfig) {}

  send() {
    throw new Error("Aeron transport not linked");
  }

  poll() {
    throw new Error("Aeron transport not linked");
  }

  close() {}
}

export { defaultActorIdCodec, encodeActorId, decodeActorId };
