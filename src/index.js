"use strict";

const path = require("node:path");
const { lookup } = require("./keys");

// Loaded lazily so argument validation and the key table can be tested on a
// machine with no compiled addon and no desktop session.
let addon = null;
function loadAddon() {
  if (addon === null) {
    addon = require("node-gyp-build")(path.join(__dirname, ".."));
  }
  return addon;
}

let listening = false;

// Convert the addon's raw event into the public KeyEvent shape. The addon
// reports { keyCode, extended, state, repeat, character }; `character` is the
// layout-translated character for printable keys, or "" when the platform has
// no translation (always the case on Linux).
function toKeyEvent(raw) {
  const entry = lookup(process.platform, raw.keyCode, raw.extended);
  return {
    key: raw.character || (entry ? entry.key : "Unidentified"),
    code: entry ? entry.code : "",
    state: raw.state,
    repeat: Boolean(raw.repeat),
  };
}

/**
 * Start listening for keyboard press and release events.
 *
 * @param {(event: import("./index").KeyEvent) => void} callback
 * @throws {TypeError} If callback is not a function.
 * @throws {Error} If already listening, or the OS listener could not be
 *   installed (missing assistive access on macOS, a failed hook on Windows,
 *   or no readable /dev/input keyboard node on Linux — see the message).
 */
function start(callback) {
  if (typeof callback !== "function") {
    throw new TypeError("keylogger.start(callback): callback must be a function");
  }
  if (listening) {
    throw new Error("keylogger.start() called while already listening; call stop() first");
  }
  const native = loadAddon();
  native.start((raw) => callback(toKeyEvent(raw)));
  listening = true;
}

/** Stop listening. Safe to call when not listening. */
function stop() {
  if (!listening) return;
  addon.stop();
  listening = false;
}

module.exports = { start, stop };
