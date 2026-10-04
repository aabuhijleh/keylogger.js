"use strict";

const test = require("node:test");
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");

const root = path.join(__dirname, "..");

test("declaration file uses export = and the KeyEvent shape", () => {
  const dts = fs.readFileSync(path.join(root, "src", "index.d.ts"), "utf8");
  assert.match(dts, /export\s*=/, "index.d.ts must use export =");
  assert.doesNotMatch(dts, /export const start/, "named exports do not match the CommonJS runtime");
  assert.match(dts, /state:\s*"down" \| "up"/);
  assert.match(dts, /repeat:\s*boolean/);
});

test("package.json exposes types through an exports map, types condition first", () => {
  const pkg = JSON.parse(fs.readFileSync(path.join(root, "package.json"), "utf8"));
  assert.ok(pkg.exports, "package.json has an exports map");
  const dot = pkg.exports["."];
  assert.ok(dot, "exports map has a '.' entry");
  assert.equal(Object.keys(dot)[0], "types", "types condition comes first");
  assert.ok(pkg.types, "top-level types field kept for older resolvers");
});

test("start rejects a missing or non-function callback with TypeError", () => {
  const keylogger = require("../src/index.js");
  assert.throws(() => keylogger.start(), TypeError);
  assert.throws(() => keylogger.start("not a function"), TypeError);
  assert.throws(() => keylogger.start({}), TypeError);
});

test("stop without start is a no-op", () => {
  const keylogger = require("../src/index.js");
  assert.doesNotThrow(() => keylogger.stop());
  assert.doesNotThrow(() => {
    keylogger.stop();
    keylogger.stop();
  });
});
