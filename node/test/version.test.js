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
      const payload = parts.length === 8 ? Number(parts[7]) : Number(parts[6]);
      messages.push({
        epoch: Number(parts[1]),
        channelId: Number(parts[2]),
        sourceId: Number(parts[3]),
        sourceSeq: Number(parts[4]),
        schemaId: Number(parts[5]),
        qos,
        payload
      });
    } else if (parts[0] === "E" && parts.length === 4) {
      expected.push({
        epoch: Number(parts[1]),
        state: Number(parts[2]),
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
    { epoch: 2, channelId: 2, sourceId: 1, sourceSeq: 2, schemaId: 100, qos: 0, payload: 5 },
    { epoch: 1, channelId: 1, sourceId: 2, sourceSeq: 1, schemaId: 100, qos: 0, payload: 2 },
    { epoch: 1, channelId: 1, sourceId: 1, sourceSeq: 2, schemaId: 100, qos: 0, payload: -1 },
    { epoch: 3, channelId: 1, sourceId: 1, sourceSeq: 1, schemaId: 100, qos: 0, payload: 4 }
  ];

  const results = epoch.processMessages(messages);
  assert.deepEqual(results, [
    { epoch: 1, state: 1, hash: "c3c43ef01be7b74f" },
    { epoch: 2, state: 6, hash: "c3c43bf01be7b236" },
    { epoch: 3, state: 10, hash: "8e2e70ff6abccccd" }
  ]);
});

test("InMemoryTransport polls and closes", () => {
  const transport = new epoch.InMemoryTransport();
  transport.send({ payload: 1 });
  transport.send({ payload: 2 });
  transport.send({ payload: 3 });

  assert.deepEqual(transport.poll(0), []);
  assert.deepEqual(transport.poll(2), [{ payload: 1 }, { payload: 2 }]);
  assert.deepEqual(transport.poll(10), [{ payload: 3 }]);

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
