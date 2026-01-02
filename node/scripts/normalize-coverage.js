const fs = require("node:fs");
const path = require("node:path");

const lcovPath = path.join(__dirname, "..", "coverage", "lcov.info");
if (!fs.existsSync(lcovPath)) {
  process.exit(0);
}

const lines = fs.readFileSync(lcovPath, "utf8").split(/\r?\n/);
const rewritten = lines.map((line) => {
  if (!line.startsWith("SF:")) {
    return line;
  }
  const filePath = line.slice(3);
  if (filePath.startsWith("/") || filePath.startsWith("node/")) {
    return line;
  }
  return `SF:node/${filePath}`;
});
fs.writeFileSync(lcovPath, rewritten.join("\n"));
