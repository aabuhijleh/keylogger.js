"use strict";

const test = require("node:test");
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");

const root = path.join(__dirname, "..");

const { table } = require("../src/keys");

// The native code and extended flag the current OS produces for a UI Events
// code, so the wrapper tests run identically on macOS, Windows, and Linux.
function nativeFor(code) {
  const row = table.find((entry) => entry.code === code);
  const platform = { darwin: "mac", win32: "win", linux: "linux" }[process.platform];
  const value = row[platform];
  assert.ok(value != null, `${code} has a ${process.platform} code`);
  if (typeof value === "object") {
    return { keyCode: value.vk, extended: value.extended };
  }
  return { keyCode: value, extended: false };
}

// A fake native addon injected through the require cache so the wrapper's
// subscription lifecycle can be tested without a compiled addon or a
// desktop session. Must be installed before src/index.js is required.
function fakeAddon() {
  const state = {
    starts: 0,
    stops: 0,
    dispatch: null,
    failNextStart: null,
    emit(raw) {
      state.dispatch(raw);
    },
    start(dispatch) {
      if (state.failNextStart) {
        const error = state.failNextStart;
        state.failNextStart = null;
        throw error;
      }
      state.starts++;
      state.dispatch = dispatch;
    },
    stop() {
      state.stops++;
      state.dispatch = null;
    },
  };
  const resolved = require.resolve("node-gyp-build");
  require.cache[resolved] = {
    id: resolved,
    filename: resolved,
    loaded: true,
    exports: () => state,
  };
  return state;
}

test("declaration file uses export = and the KeyEvent shape", () => {
  const dts = fs.readFileSync(path.join(root, "src", "index.d.ts"), "utf8");
  assert.match(dts, /export\s*=/, "index.d.ts must use export =");
  assert.doesNotMatch(dts, /export const start/, "named exports do not match the CommonJS runtime");
  assert.match(dts, /state:\s*"down" \| "up"/);
  assert.match(dts, /repeat:\s*boolean/);
  assert.match(dts, /listen\(handler/, "the public API is listen(handler)");
});

test("package.json exposes types through an exports map, types condition first", () => {
  const pkg = JSON.parse(fs.readFileSync(path.join(root, "package.json"), "utf8"));
  assert.ok(pkg.exports, "package.json has an exports map");
  const dot = pkg.exports["."];
  assert.ok(dot, "exports map has a '.' entry");
  assert.equal(Object.keys(dot)[0], "types", "types condition comes first");
  assert.ok(pkg.types, "top-level types field kept for older resolvers");
});

test("listen rejects a missing or non-function handler with TypeError", () => {
  fakeAddon();
  const keylogger = require("../src/index.js");
  assert.throws(() => keylogger.listen(), TypeError);
  assert.throws(() => keylogger.listen("not a function"), TypeError);
  assert.throws(() => keylogger.listen({}), TypeError);
});

test("first subscriber starts the native listener once; events fan out", () => {
  const addon = fakeAddon();
  delete require.cache[require.resolve("../src/index.js")];
  const keylogger = require("../src/index.js");

  const seen1 = [];
  const seen2 = [];
  keylogger.listen((event) => seen1.push(event));
  keylogger.listen((event) => seen2.push(event));
  assert.equal(addon.starts, 1, "native listener installed once for two subscribers");

  addon.emit({ ...nativeFor("Space"), state: "down", repeat: false, character: "" });
  const expected = { key: " ", code: "Space", state: "down", repeat: false };
  assert.deepEqual(seen1, [expected]);
  assert.deepEqual(seen2, [expected]);
});

test("last unsubscribe stops the native listener; unsubscribe is idempotent", () => {
  const addon = fakeAddon();
  delete require.cache[require.resolve("../src/index.js")];
  const keylogger = require("../src/index.js");

  const seen = [];
  const unlisten1 = keylogger.listen(() => {});
  const unlisten2 = keylogger.listen((event) => seen.push(event));

  unlisten1();
  addon.emit({ ...nativeFor("Space"), state: "down", repeat: false, character: "" });
  assert.equal(seen.length, 1, "remaining subscriber still receives events");
  assert.equal(addon.stops, 0, "native listener kept while a subscriber remains");

  unlisten2();
  unlisten2();
  assert.equal(addon.stops, 1, "native listener stopped exactly once");
});

test("a throwing handler does not starve other subscribers", () => {
  const addon = fakeAddon();
  delete require.cache[require.resolve("../src/index.js")];
  const keylogger = require("../src/index.js");

  const seen = [];
  keylogger.listen(() => {
    throw new Error("handler bug");
  });
  keylogger.listen((event) => seen.push(event));
  addon.emit({ ...nativeFor("KeyA"), state: "down", repeat: false, character: "" });
  assert.equal(seen.length, 1);
});

test("a failed native start leaves no dangling subscription state", () => {
  const addon = fakeAddon();
  delete require.cache[require.resolve("../src/index.js")];
  const keylogger = require("../src/index.js");

  addon.failNextStart = new Error("keylogger: permission denied");
  assert.throws(() => keylogger.listen(() => {}), /permission denied/);

  // The next listen must install cleanly rather than believing it is
  // already listening.
  const seen = [];
  keylogger.listen((event) => seen.push(event));
  assert.equal(addon.starts, 1);
  addon.emit({ ...nativeFor("KeyA"), state: "down", repeat: false, character: "" });
  assert.equal(seen.length, 1);
});

test("layout-translated characters override the table key", () => {
  const addon = fakeAddon();
  delete require.cache[require.resolve("../src/index.js")];
  const keylogger = require("../src/index.js");

  const seen = [];
  keylogger.listen((event) => seen.push(event));
  addon.emit({ ...nativeFor("KeyA"), state: "down", repeat: false, character: "A" });
  assert.equal(seen[0].key, "A", "native character wins over the table default");
  assert.equal(seen[0].code, "KeyA");
});

test("unknown native codes degrade to Unidentified with an empty code", () => {
  const addon = fakeAddon();
  delete require.cache[require.resolve("../src/index.js")];
  const keylogger = require("../src/index.js");

  const seen = [];
  keylogger.listen((event) => seen.push(event));
  addon.emit({ keyCode: 65000, extended: false, state: "down", repeat: false, character: "" });
  assert.deepEqual(seen[0], { key: "Unidentified", code: "", state: "down", repeat: false });
});
