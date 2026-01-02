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
