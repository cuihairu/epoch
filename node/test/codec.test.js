const test = require("node:test");
const assert = require("node:assert/strict");

const epoch = require("../dist/index.js");

test("actorId codec rejects out-of-range parts", () => {
  const cases = [
    { region: -1, server: 0, processType: 0, processIndex: 0, actorIndex: 0 },
    { region: 1024, server: 0, processType: 0, processIndex: 0, actorIndex: 0 },
    { region: 0, server: -1, processType: 0, processIndex: 0, actorIndex: 0 },
    { region: 0, server: 4096, processType: 0, processIndex: 0, actorIndex: 0 },
    { region: 0, server: 0, processType: 64, processIndex: 0, actorIndex: 0 },
    { region: 0, server: 0, processType: 0, processIndex: 1024, actorIndex: 0 },
    { region: 0, server: 0, processType: 0, processIndex: 0, actorIndex: 1 << 26 },
  ];
  for (const parts of cases) {
    assert.throws(() => epoch.encodeActorId(parts), RangeError);
  }
});

test("processMessages breaks sourceId and sourceSeq ties", () => {
  const messages = [
    { epoch: 3, channelId: 1, qos: 2, sourceId: 9, sourceSeq: 2, payload: 20 },
    { epoch: 3, channelId: 1, qos: 2, sourceId: 9, sourceSeq: 1, payload: 10 },
  ];
  const results = epoch.processMessages(messages);
  assert.equal(results.length, 1);
  assert.equal(results[0].state, 30);
  assert.match(results[0].hash, /^[0-9a-f]{16}$/);
});
