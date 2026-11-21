const test = require("node:test");
const assert = require("node:assert/strict");

const fs = require("node:fs");
const path = require("node:path");
const epoch = require("../dist/index.js");

test("vector matches expected", () => {
  const vectorPath = locateVectorFile();
  const lines = fs.readFileSync(vectorPath, "utf8").split(/\r?\n/);
  const messages = [];
  const expected = [];

  for (const line of lines) {
    const trimmed = line.trim();
    if (!trimmed || trimmed.startsWith("#")) {
      continue;
    }
    const parts = trimmed.split(",");
    if (parts[0] === "M" && (parts.length === 7 || parts.length === 8)) {
      const qos = parts.length === 8 ? Number(parts[6]) : 0;
      const payload = parts.length === 8 ? BigInt(parts[7]) : BigInt(parts[6]);
      messages.push({
        epoch: BigInt(parts[1]),
        channelId: BigInt(parts[2]),
        sourceId: BigInt(parts[3]),
        sourceSeq: BigInt(parts[4]),
        schemaId: BigInt(parts[5]),
        qos,
        payload
      });
    } else if (parts[0] === "E" && parts.length === 4) {
      expected.push({
        epoch: BigInt(parts[1]),
        state: BigInt(parts[2]),
        hash: parts[3]
      });
    }
  }

  const results = epoch.processMessages(messages);
  assert.equal(results.length, expected.length);
  for (let i = 0; i < expected.length; i++) {
    assert.equal(results[i].epoch, expected[i].epoch);
    assert.equal(results[i].state, expected[i].state);
    assert.equal(results[i].hash, expected[i].hash);
  }
});

test("version matches expected", () => {
  assert.equal(epoch.version(), "0.1.0");
});

test("fnv1a64Hex matches known vectors", () => {
  assert.equal(epoch.fnv1a64Hex("state:0"), "c3c43df01be7b59c");
  assert.equal(epoch.fnv1a64Hex("hello"), "a430d84680aabd0b");
});

test("processMessages handles empty input", () => {
  assert.deepEqual(epoch.processMessages([]), []);
});

test("processMessages orders epochs and carries state", () => {
  const messages = [
    { epoch: 2n, channelId: 2n, sourceId: 1n, sourceSeq: 2n, schemaId: 100n, qos: 0, payload: 5n },
    { epoch: 1n, channelId: 1n, sourceId: 2n, sourceSeq: 1n, schemaId: 100n, qos: 0, payload: 2n },
    { epoch: 1n, channelId: 1n, sourceId: 1n, sourceSeq: 2n, schemaId: 100n, qos: 0, payload: -1n },
    { epoch: 3n, channelId: 1n, sourceId: 1n, sourceSeq: 1n, schemaId: 100n, qos: 0, payload: 4n }
  ];

  const results = epoch.processMessages(messages);
  assert.deepEqual(results, [
    { epoch: 1n, state: 1n, hash: "c3c43ef01be7b74f" },
    { epoch: 2n, state: 6n, hash: "c3c43bf01be7b236" },
    { epoch: 3n, state: 10n, hash: "8e2e70ff6abccccd" }
  ]);
});

test("processMessages orders by qos descending within same epoch and channel", () => {
  const messages = [
    { epoch: 1n, channelId: 1n, sourceId: 1n, sourceSeq: 1n, schemaId: 100n, qos: 1, payload: 10n },
    { epoch: 1n, channelId: 1n, sourceId: 1n, sourceSeq: 2n, schemaId: 100n, qos: 5, payload: 20n },
    { epoch: 1n, channelId: 1n, sourceId: 1n, sourceSeq: 3n, schemaId: 100n, qos: 3, payload: 30n }
  ];

  const results = epoch.processMessages(messages);
  assert.equal(results.length, 1);
  assert.equal(results[0].epoch, 1n);
  assert.equal(results[0].state, 60n);
  assert.equal(results[0].hash, epoch.fnv1a64Hex("state:60"));
});

test("processMessages keeps full 64-bit precision", () => {
  // 2^53 + 1 is not representable as a number: the old number-typed payload
  // silently rounded it and broke the cross-language stateHash contract.
  const big = 9007199254740993n;
  const messages = [
    { epoch: 1n, channelId: 1n, sourceId: 1n, sourceSeq: 1n, schemaId: 1n, qos: 0, payload: big }
  ];
  const results = epoch.processMessages(messages);
  assert.equal(results[0].state, big);
  assert.equal(results[0].hash, epoch.fnv1a64Hex(`state:${big}`));
});

test("InMemoryTransport polls and closes", () => {
  const transport = new epoch.InMemoryTransport();
  transport.send({ payload: 1n });
  transport.send({ payload: 2n });
  transport.send({ payload: 3n });

  assert.deepEqual(transport.poll(0), []);
  assert.deepEqual(transport.poll(2), [{ payload: 1n }, { payload: 2n }]);
  assert.deepEqual(transport.poll(10), [{ payload: 3n }]);

  transport.close();
  assert.deepEqual(transport.poll(1), []);
});

test("actorId default codec", () => {
  const parts = { region: 1, server: 2, processType: 3, processIndex: 4, actorIndex: 5 };
  const value = epoch.encodeActorId(parts);
  const decoded = epoch.decodeActorId(value);
  assert.deepEqual(decoded, parts);
});

function locateVectorFile() {
  let current = process.cwd();
  for (let i = 0; i < 6; i++) {
    const candidate = path.join(current, "test-vectors", "epoch_vector_v1.txt");
    if (fs.existsSync(candidate)) {
      return candidate;
    }
    const parent = path.dirname(current);
    if (parent === current) {
      break;
    }
    current = parent;
  }
  throw new Error("Vector file not found");
}
