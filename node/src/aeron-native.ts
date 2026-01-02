import * as fs from "node:fs";
import * as path from "node:path";

import type { AeronConfig } from "./index";

export type AeronStats = {
  sentCount: number;
  receivedCount: number;
  offerBackPressure: number;
  offerNotConnected: number;
  offerAdminAction: number;
  offerClosed: number;
  offerMaxPosition: number;
  offerFailed: number;
};

export type AeronNative = {
  open(config: AeronConfig): unknown;
  send(handle: unknown, frame: Buffer): void;
  poll(handle: unknown, max: number): Buffer[];
  stats(handle: unknown): AeronStats;
  close(handle: unknown): void;
};

const FRAME_LENGTH = 56;

export function loadAeronNative(koffiImpl?: any, libraryPath?: string): AeronNative {
  const koffi = koffiImpl ?? (require("koffi") as any);

  const AeronConfigStruct = koffi.struct("epoch_aeron_config_t", {
    channel: "const char *",
    stream_id: "int32_t",
    aeron_directory: "const char *",
    fragment_limit: "int32_t",
    offer_max_attempts: "int32_t"
  });
  const AeronStatsStruct = koffi.struct("epoch_aeron_stats_t", {
    sent_count: "int64_t",
    received_count: "int64_t",
    offer_back_pressure: "int64_t",
    offer_not_connected: "int64_t",
    offer_admin_action: "int64_t",
    offer_closed: "int64_t",
    offer_max_position: "int64_t",
    offer_failed: "int64_t"
  });
  const AeronHandle = koffi.pointer("epoch_aeron_transport_t", koffi.opaque());

  const lib = koffi.load(libraryPath ?? resolveLibraryPath());
  const openFn = lib.func(
    "epoch_aeron_open",
    AeronHandle,
    ["epoch_aeron_config_t *", "char *", "size_t"]
  );
  const sendFn = lib.func(
    "epoch_aeron_send",
    "int",
    [AeronHandle, "const uint8_t *", "size_t", "char *", "size_t"]
  );
  const pollFn = lib.func(
    "epoch_aeron_poll",
    "int",
    [AeronHandle, "uint8_t *", "size_t", "size_t *", "char *", "size_t"]
  );
  const statsFn = lib.func(
    "epoch_aeron_stats",
    "int",
    [AeronHandle, "epoch_aeron_stats_t *"]
  );
  const closeFn = lib.func("epoch_aeron_close", "void", [AeronHandle]);

  const trimCString = (buffer: Buffer) => {
    const end = buffer.indexOf(0);
    return buffer.toString("utf8", 0, end >= 0 ? end : buffer.length);
  };

  return {
    open(config: AeronConfig) {
      const errBuf = Buffer.alloc(256);
      const cfg = {
        channel: config.channel,
        stream_id: config.streamId,
        aeron_directory: config.aeronDirectory || null,
        fragment_limit: config.fragmentLimit ?? 64,
        offer_max_attempts: config.offerMaxAttempts ?? 10
      };
      const handle = openFn(cfg, errBuf, errBuf.length);
      if (!handle) {
        throw new Error(trimCString(errBuf));
      }
      return handle;
    },
    send(handle: unknown, frame: Buffer) {
      const errBuf = Buffer.alloc(256);
      const result = sendFn(handle, frame, frame.length, errBuf, errBuf.length);
      if (result < 0) {
        throw new Error(trimCString(errBuf));
      }
    },
    poll(handle: unknown, max: number) {
      if (!max) {
        return [];
      }
      const errBuf = Buffer.alloc(256);
      const frameBuf = Buffer.alloc(max * FRAME_LENGTH);
      const countRef = [0];
      const result = pollFn(handle, frameBuf, max, countRef, errBuf, errBuf.length);
      if (result < 0) {
        throw new Error(trimCString(errBuf));
      }
      const count = countRef[0] ?? 0;
      const frames: Buffer[] = [];
      for (let i = 0; i < count; i++) {
        const start = i * FRAME_LENGTH;
        frames.push(Buffer.from(frameBuf.subarray(start, start + FRAME_LENGTH)));
      }
      return frames;
    },
    stats(handle: unknown) {
      const stats: Record<string, number> = {};
      const result = statsFn(handle, stats);
      if (result < 0) {
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
      return {
        sentCount: Number(stats.sent_count ?? 0),
        receivedCount: Number(stats.received_count ?? 0),
        offerBackPressure: Number(stats.offer_back_pressure ?? 0),
        offerNotConnected: Number(stats.offer_not_connected ?? 0),
        offerAdminAction: Number(stats.offer_admin_action ?? 0),
        offerClosed: Number(stats.offer_closed ?? 0),
        offerMaxPosition: Number(stats.offer_max_position ?? 0),
        offerFailed: Number(stats.offer_failed ?? 0)
      };
    },
    close(handle: unknown) {
      closeFn(handle);
    }
  };
}

function resolveLibraryPath(): string {
  const env = process.env.EPOCH_AERON_LIBRARY;
  if (env) {
    return env;
  }

  const filename = process.platform === "darwin"
    ? "libepoch_aeron.dylib"
    : process.platform === "win32"
      ? "epoch_aeron.dll"
      : "libepoch_aeron.so";

  let current = process.cwd();
  for (let i = 0; i < 6; i++) {
    const candidate = path.resolve(current, "native", "build", filename);
    if (fs.existsSync(candidate)) {
      return candidate;
    }
    const parent = path.dirname(current);
    if (parent === current) {
      break;
    }
    current = parent;
  }

  return "epoch_aeron";
}
