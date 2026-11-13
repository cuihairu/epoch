const test = require("node:test");
const assert = require("node:assert/strict");

const epoch = require("../dist/index.js");
const { loadAeronNative } = require("../dist/aeron-native.js");

class FakeNative {
  constructor() {
    this.frames = [];
  }

  open() {
    return {};
  }

  send(_handle, frame) {
    this.frames.push(Buffer.from(frame));
  }

  poll(_handle, max) {
    const out = this.frames.slice(0, max);
    this.frames = this.frames.slice(max);
    return out;
  }

  stats() {
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

  close() {}
}

class ErrorNative extends FakeNative {
  open() { return {}; }
  send() { throw new Error("send failed"); }
  poll() { throw new Error("poll failed"); }
  stats() {
    return {
      sentCount: 1,
      receivedCount: 2,
      offerBackPressure: 3,
      offerNotConnected: 4,
      offerAdminAction: 5,
      offerClosed: 6,
      offerMaxPosition: 7,
      offerFailed: 8
    };
  }
}

test("Aeron frame round trip", () => {
  const message = {
    epoch: 1,
    channelId: 2,
    sourceId: 3,
    sourceSeq: 4,
    schemaId: 5,
    qos: 6,
    payload: -7
  };
  const frame = epoch.encodeAeronFrame(message);
  const decoded = epoch.decodeAeronFrame(frame);
  assert.deepEqual(decoded, message);
});

test("Aeron frame rejects short buffer", () => {
  assert.throws(() => epoch.decodeAeronFrame(Buffer.from([1, 0])));
});

test("Aeron frame rejects unsupported version", () => {
  const frame = epoch.encodeAeronFrame({
    epoch: 1, channelId: 1, sourceId: 1, sourceSeq: 1, schemaId: 1, qos: 1, payload: 1
  });
  frame.writeUInt8(2, 0);
  assert.throws(() => epoch.decodeAeronFrame(frame), /Unsupported/);
});

test("AeronTransport uses native adapter", () => {
  const transport = new epoch.AeronTransport(
    { channel: "aeron:ipc", streamId: 1, aeronDirectory: "" },
    new FakeNative()
  );
  transport.send({ epoch: 1, channelId: 1, sourceId: 1, sourceSeq: 1, schemaId: 1, qos: 0, payload: 9 });
  transport.send({ epoch: 1, channelId: 1, sourceId: 1, sourceSeq: 2, schemaId: 1, qos: 0, payload: 10 });

  assert.deepEqual(transport.poll(1).map((m) => m.payload), [9]);
  assert.deepEqual(transport.poll(5).map((m) => m.payload), [10]);
  transport.close();
  assert.deepEqual(transport.poll(1), []);
});

test("AeronTransport exposes config and rejects operations after close", () => {
  const config = { channel: "aeron:ipc", streamId: 2, aeronDirectory: "" };
  const native = new ErrorNative();
  const transport = new epoch.AeronTransport(config, native);
  assert.equal(transport.config, config);
  assert.equal(transport.stats().sentCount, 1);
  assert.throws(() => transport.send({
    epoch: 1, channelId: 1, sourceId: 1, sourceSeq: 1, schemaId: 1, qos: 1, payload: 1
  }), /send failed/);
  assert.throws(() => transport.poll(1), /poll failed/);
  transport.close();
  transport.close();
  assert.throws(() => transport.send({
    epoch: 1, channelId: 1, sourceId: 1, sourceSeq: 1, schemaId: 1, qos: 1, payload: 1
  }), /closed/);
  assert.deepEqual(transport.poll(1), []);
  assert.equal(transport.stats().sentCount, 0);
});

test("Aeron native adapter wrapper", () => {
  const fakeKoffi = {
    struct() {
      return {};
    },
    pointer() {
      return "ptr";
    },
    opaque() {
      return {};
    },
    load() {
      return {
        func(name) {
          if (name === "epoch_aeron_open") {
            return () => 1;
          }
          if (name === "epoch_aeron_send") {
            return () => 0;
          }
          if (name === "epoch_aeron_poll") {
            return (_handle, frameBuf, _max, countRef) => {
              frameBuf.fill(0);
              countRef[0] = 1;
              return 0;
            };
          }
          if (name === "epoch_aeron_stats") {
            return (_handle, stats) => {
              stats.sent_count = 1;
              stats.received_count = 2;
              stats.offer_back_pressure = 3;
              stats.offer_not_connected = 4;
              stats.offer_admin_action = 5;
              stats.offer_closed = 6;
              stats.offer_max_position = 7;
              stats.offer_failed = 8;
              return 0;
            };
          }
          if (name === "epoch_aeron_close") {
            return () => {};
          }
          throw new Error(`unexpected func ${name}`);
        }
      };
    }
  };

  const previous = process.env.EPOCH_AERON_LIBRARY;
  process.env.EPOCH_AERON_LIBRARY = "/tmp/libepoch_aeron.so";
  const native = loadAeronNative(fakeKoffi);
  const handle = native.open({ channel: "aeron:ipc", streamId: 1, aeronDirectory: "" });
  native.send(handle, Buffer.alloc(56));
  const frames = native.poll(handle, 1);
  assert.equal(frames.length, 1);
  const stats = native.stats(handle);
  assert.equal(stats.sentCount, 1);
  assert.equal(stats.receivedCount, 2);
  native.close(handle);
  if (previous === undefined) {
    delete process.env.EPOCH_AERON_LIBRARY;
  } else {
    process.env.EPOCH_AERON_LIBRARY = previous;
  }
});

test("Aeron native adapter maps native errors and zero batches", () => {
  const calls = {};
  const fakeKoffi = {
    struct() { return {}; },
    pointer() { return "ptr"; },
    opaque() { return {}; },
    load() {
      return {
        func(name) {
          calls[name] = calls[name] || 0;
          calls[name]++;
          if (name === "epoch_aeron_open") return () => "handle";
          if (name === "epoch_aeron_send") return (_handle, _frame, _len, errBuf) => {
            errBuf.write("send failed\0");
            return -1;
          };
          if (name === "epoch_aeron_poll") {
            const pollCall = () => ++calls.epoch_aeron_poll;
            return (_handle, _frames, _max, countRef, errBuf) => {
              const current = pollCall();
              if (current === 2) {
                errBuf.write("poll failed\0");
                return -1;
              }
              countRef[0] = 0;
              return 0;
            };
          }
          if (false) {
            if (calls[name] === 4) {
              errBuf.write("poll failed\0");
              return -1;
            }
            countRef[0] = 0;
            return 0;
          };
          if (name === "epoch_aeron_stats") {
            const statsCall = () => ++calls.epoch_aeron_stats;
            return (_handle, stats) => {
              const current = statsCall();
              if (current === 2) return -1;
              stats.sent_count = 9;
              return 0;
            };
          }
          if (name === "epoch_aeron_close") return () => {};
          throw new Error(`unexpected func ${name}`);
        }
      };
    }
  };

  const previous = process.env.EPOCH_AERON_LIBRARY;
  process.env.EPOCH_AERON_LIBRARY = "/tmp/libepoch_aeron.so";
  try {
    const native = loadAeronNative(fakeKoffi);
    assert.throws(() => loadAeronNative({ struct() { return {}; }, pointer() { return "ptr"; }, opaque() { return {}; }, load() { throw new Error("load failed"); } }), /load failed/);
    const handle = native.open({ channel: "aeron:ipc", streamId: 1, aeronDirectory: "" });
    assert.throws(() => native.send(handle, Buffer.alloc(56)), /send failed/);
    assert.throws(() => native.poll(handle, 1), /poll failed/);
    assert.deepEqual(native.poll(handle, 1), []);
    assert.deepEqual(native.poll(handle, 0), []);
    const failingStats = native.stats(handle);
    assert.equal(failingStats.sentCount, 0);
    assert.deepEqual(failingStats, {
      sentCount: 0, receivedCount: 0, offerBackPressure: 0, offerNotConnected: 0,
      offerAdminAction: 0, offerClosed: 0, offerMaxPosition: 0, offerFailed: 0,
      receivedErrors: 0
    });
    const successStats = native.stats(handle);
    assert.equal(successStats.sentCount, 9);
    assert.deepEqual(successStats, {
      sentCount: 9, receivedCount: 0, offerBackPressure: 0, offerNotConnected: 0,
      offerAdminAction: 0, offerClosed: 0, offerMaxPosition: 0, offerFailed: 0,
      receivedErrors: 0
    });
    native.close(handle);
  } finally {
    if (previous === undefined) delete process.env.EPOCH_AERON_LIBRARY;
    else process.env.EPOCH_AERON_LIBRARY = previous;
  }
});

test("Aeron native adapter discovers local library and normalizes config", () => {
  const configs = [];
  const fakeKoffi = {
    struct() { return {}; },
    pointer() { return "ptr"; },
    opaque() { return {}; },
    load(libraryPath) {
      assert.match(libraryPath, /native\/build\/libepoch_aeron\.so$/);
      return { func(name) {
        if (name === "epoch_aeron_open") return (config) => {
          configs.push(config);
          return "handle";
        };
        if (name === "epoch_aeron_send") return () => 0;
        if (name === "epoch_aeron_poll") return () => 0;
        if (name === "epoch_aeron_stats") return () => 0;
        if (name === "epoch_aeron_close") return () => {};
        throw new Error(`unexpected func ${name}`);
      } };
    }
  };

  const previous = process.env.EPOCH_AERON_LIBRARY;
  delete process.env.EPOCH_AERON_LIBRARY;
  try {
    const native = loadAeronNative(fakeKoffi);
    native.open({ channel: "aeron:ipc", streamId: 1, aeronDirectory: undefined, fragmentLimit: undefined, offerMaxAttempts: undefined });
    assert.equal(configs[0].aeron_directory, null);
    assert.equal(configs[0].fragment_limit, 64);
    assert.equal(configs[0].offer_max_attempts, 10);
  } finally {
    if (previous === undefined) delete process.env.EPOCH_AERON_LIBRARY;
    else process.env.EPOCH_AERON_LIBRARY = previous;
  }
});

test("Aeron native adapter falls back to library name when no local build", () => {
  const fs = require("node:fs");
  const seen = [];
  const fakeKoffi = {
    struct() { return {}; },
    pointer() { return "ptr"; },
    opaque() { return {}; },
    load(libraryPath) {
      seen.push(libraryPath);
      return { func(name) {
        if (name === "epoch_aeron_open") return () => "handle";
        if (name === "epoch_aeron_send") return () => 0;
        if (name === "epoch_aeron_poll") return () => 0;
        if (name === "epoch_aeron_stats") return () => 0;
        if (name === "epoch_aeron_close") return () => {};
        throw new Error(`unexpected func ${name}`);
      } };
    }
  };

  const previousEnv = process.env.EPOCH_AERON_LIBRARY;
  const originalExists = fs.existsSync;
  delete process.env.EPOCH_AERON_LIBRARY;
  fs.existsSync = () => false;
  try {
    const native = loadAeronNative(fakeKoffi);
    native.open({ channel: "aeron:ipc", streamId: 1, aeronDirectory: undefined, fragmentLimit: undefined, offerMaxAttempts: undefined });
    assert.equal(seen.length, 1);
    assert.equal(seen[0], "epoch_aeron");
  } finally {
    fs.existsSync = originalExists;
    if (previousEnv === undefined) delete process.env.EPOCH_AERON_LIBRARY;
    else process.env.EPOCH_AERON_LIBRARY = previousEnv;
  }
});

test("Aeron native adapter error buffer handles unsupported version", () => {
  const frame = epoch.encodeAeronFrame({
    epoch: 1, channelId: 1, sourceId: 1, sourceSeq: 1, schemaId: 1, qos: 1, payload: 1
  });
  frame[0] = 2;
  const fakeKoffi = {
    struct() { return {}; },
    pointer() { return "ptr"; },
    opaque() { return {}; },
    load() {
      return { func(name) {
        if (name === "epoch_aeron_open") return () => "handle";
        if (name === "epoch_aeron_send") return () => 0;
          if (name === "epoch_aeron_poll") return (_handle, frameBuf, _max, countRef) => {
          frame.copy(frameBuf);
          countRef[0] = 1;
          return 0;
        };
        if (name === "epoch_aeron_stats") return () => 0;
        if (name === "epoch_aeron_close") return () => {};
        throw new Error(`unexpected func ${name}`);
      } };
    }
  };
  const previous = process.env.EPOCH_AERON_LIBRARY;
  process.env.EPOCH_AERON_LIBRARY = "/tmp/libepoch_aeron.so";
  try {
    const native = loadAeronNative(fakeKoffi);
    const handle = native.open({ channel: "aeron:ipc", streamId: 1, aeronDirectory: "" });
    const frames = native.poll(handle, 1);
    assert.equal(frames.length, 1);
    assert.throws(() => epoch.decodeAeronFrame(frames[0]), /Unsupported/);
  } finally {
    if (previous === undefined) delete process.env.EPOCH_AERON_LIBRARY;
    else process.env.EPOCH_AERON_LIBRARY = previous;
  }
});

test("Aeron native adapter propagates open errors", () => {
  const expected = new Error("open failed");
  const fakeKoffi = {
    struct() { return {}; },
    pointer() { return "ptr"; },
    opaque() { return {}; },
    load() {
      return {
        func(name) {
          if (name === "epoch_aeron_open") {
            return () => 0;
          }
          if (name === "epoch_aeron_send") return () => 0;
          if (name === "epoch_aeron_poll") return () => 0;
          if (name === "epoch_aeron_stats") return () => 0;
          if (name === "epoch_aeron_close") return () => {};
          throw expected;
        }
      };
    }
  };
  const previous = process.env.EPOCH_AERON_LIBRARY;
  process.env.EPOCH_AERON_LIBRARY = "/tmp/libepoch_aeron.so";
  try {
    const native = loadAeronNative(fakeKoffi);
  assert.throws(() => native.open({ channel: "aeron:ipc", streamId: 1, aeronDirectory: "" }), Error);
  } finally {
    if (previous === undefined) delete process.env.EPOCH_AERON_LIBRARY;
    else process.env.EPOCH_AERON_LIBRARY = previous;
  }
});

test("AeronTransport skips poison frames and counts errors", () => {
  const native = new FakeNative();
  const transport = new epoch.AeronTransport(
    { channel: "aeron:ipc", streamId: 7, aeronDirectory: "" },
    native
  );
  const good = epoch.encodeAeronFrame({
    epoch: 1, channelId: 1, sourceId: 1, sourceSeq: 3, schemaId: 1, qos: 0, payload: 99
  });
  const bad = Buffer.from(good);
  bad.writeUInt8(2, 0);
  native.frames.push(Buffer.from(bad), Buffer.from(bad), good);

  assert.deepEqual(transport.poll(4).map((m) => m.payload), [99]);
  const stats = transport.stats();
  assert.equal(stats.receivedErrors, 2);
  transport.close();
  assert.equal(transport.stats().receivedErrors, 2);
});
