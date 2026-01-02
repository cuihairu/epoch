export type ActorIdParts = {
  region: number;
  server: number;
  processType: number;
  processIndex: number;
  actorIndex: number;
};

export type ActorIdCodec = {
  name: string;
  encode(parts: ActorIdParts): bigint;
  decode(value: bigint): ActorIdParts;
};

const REGION_BITS = 10n;
const SERVER_BITS = 12n;
const PROCESS_TYPE_BITS = 6n;
const PROCESS_INDEX_BITS = 10n;
const ACTOR_INDEX_BITS = 26n;

const REGION_MAX = (1n << REGION_BITS) - 1n;
const SERVER_MAX = (1n << SERVER_BITS) - 1n;
const PROCESS_TYPE_MAX = (1n << PROCESS_TYPE_BITS) - 1n;
const PROCESS_INDEX_MAX = (1n << PROCESS_INDEX_BITS) - 1n;
const ACTOR_INDEX_MAX = (1n << ACTOR_INDEX_BITS) - 1n;

const ACTOR_INDEX_SHIFT = 0n;
const PROCESS_INDEX_SHIFT = ACTOR_INDEX_SHIFT + ACTOR_INDEX_BITS;
const PROCESS_TYPE_SHIFT = PROCESS_INDEX_SHIFT + PROCESS_INDEX_BITS;
const SERVER_SHIFT = PROCESS_TYPE_SHIFT + PROCESS_TYPE_BITS;
const REGION_SHIFT = SERVER_SHIFT + SERVER_BITS;

export const defaultActorIdCodec: ActorIdCodec = {
  name: "default",
  encode(parts) {
    const region = BigInt(parts.region);
    const server = BigInt(parts.server);
    const processType = BigInt(parts.processType);
    const processIndex = BigInt(parts.processIndex);
    const actorIndex = BigInt(parts.actorIndex);

    if (region < 0n || region > REGION_MAX ||
        server < 0n || server > SERVER_MAX ||
        processType < 0n || processType > PROCESS_TYPE_MAX ||
        processIndex < 0n || processIndex > PROCESS_INDEX_MAX ||
        actorIndex < 0n || actorIndex > ACTOR_INDEX_MAX) {
      throw new RangeError("ActorId parts out of range");
    }

    return (region << REGION_SHIFT) |
      (server << SERVER_SHIFT) |
      (processType << PROCESS_TYPE_SHIFT) |
      (processIndex << PROCESS_INDEX_SHIFT) |
      (actorIndex << ACTOR_INDEX_SHIFT);
  },
  decode(value) {
    const raw = BigInt(value);
    return {
      region: Number((raw >> REGION_SHIFT) & REGION_MAX),
      server: Number((raw >> SERVER_SHIFT) & SERVER_MAX),
      processType: Number((raw >> PROCESS_TYPE_SHIFT) & PROCESS_TYPE_MAX),
      processIndex: Number((raw >> PROCESS_INDEX_SHIFT) & PROCESS_INDEX_MAX),
      actorIndex: Number((raw >> ACTOR_INDEX_SHIFT) & ACTOR_INDEX_MAX)
    };
  }
};

export function encodeActorId(parts: ActorIdParts, codec: ActorIdCodec = defaultActorIdCodec): bigint {
  return codec.encode(parts);
}

export function decodeActorId(value: bigint, codec: ActorIdCodec = defaultActorIdCodec): ActorIdParts {
  return codec.decode(value);
}
