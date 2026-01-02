function version() {
  return "0.1.0";
}

const actorId = require("./actor-id");

const FNV_OFFSET_BASIS = 0xcbf29ce484222325n;
const FNV_PRIME = 0x100000001b3n;
const FNV_MASK = 0xffffffffffffffffn;

function fnv1a64Hex(input) {
  let hash = FNV_OFFSET_BASIS;
  const bytes = Buffer.from(input, "utf8");
  for (const b of bytes) {
    hash ^= BigInt(b);
    hash = (hash * FNV_PRIME) & FNV_MASK;
  }
  return hash.toString(16).padStart(16, "0");
}

function processMessages(messages) {
  messages.sort((a, b) => {
    if (a.epoch !== b.epoch) return a.epoch - b.epoch;
    if (a.channelId !== b.channelId) return a.channelId - b.channelId;
    const qosA = a.qos ?? 0;
    const qosB = b.qos ?? 0;
    if (qosA !== qosB) return qosB - qosA;
    if (a.sourceId !== b.sourceId) return a.sourceId - b.sourceId;
    return a.sourceSeq - b.sourceSeq;
  });

  const results = [];
  let state = 0;
  let currentEpoch = null;

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

class InMemoryTransport {
  constructor() {
    this.queue = [];
  }

  send(message) {
    this.queue.push(message);
  }

  poll(max) {
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

class AeronTransport {
  constructor(config) {
    this.config = config;
  }

  send() {
    throw new Error("Aeron transport not linked");
  }

  poll() {
    throw new Error("Aeron transport not linked");
  }

  close() {}
}

module.exports = {
  version,
  fnv1a64Hex,
  processMessages,
  InMemoryTransport,
  AeronTransport,
  defaultActorIdCodec: actorId.defaultActorIdCodec,
  encodeActorId: actorId.encodeActorId,
  decodeActorId: actorId.decodeActorId
};
