const test = require("node:test");
const assert = require("node:assert/strict");

const fs = require("node:fs");
const path = require("node:path");
const epoch = require("../index.js");

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
    if (parts[0] === "M" && parts.length === 7) {
      messages.push({
        epoch: Number(parts[1]),
        channelId: Number(parts[2]),
        sourceId: Number(parts[3]),
        sourceSeq: Number(parts[4]),
        schemaId: Number(parts[5]),
        payload: Number(parts[6])
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
