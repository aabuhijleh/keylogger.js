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

// Subscribers to fan events out to. The first subscriber installs the OS
// listener and the last unsubscribe tears it down, so any number of
// independent consumers can listen at once.
const handlers = new Set();
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

function dispatch(raw) {
  const event = toKeyEvent(raw);
  for (const handler of handlers) {
    try {
      handler(event);
    } catch {
      // A throwing handler must not starve the other subscribers or reach
      // the native side. Catch handler errors yourself if you need them.
    }
  }
}

/**
 * Subscribe to keyboard press and release events.
 *
 * The first subscriber installs the OS listener; when the last subscription
 * is removed the listener is torn down and its native resources freed.
 *
 * @param {(event: import("./index").KeyEvent) => void} handler
 * @returns {() => void} A function that removes this subscription. Idempotent.
 * @throws {TypeError} If handler is not a function.
 * @throws {Error} If the OS listener could not be installed (missing
 *   assistive access on macOS, a failed hook on Windows, or no readable
 *   /dev/input keyboard node on Linux — the message names the permission).
 */
function listen(handler) {
  if (typeof handler !== "function") {
    throw new TypeError("keylogger.listen(handler): handler must be a function");
  }
  if (!listening) {
    loadAddon().start(dispatch);
    listening = true;
  }
  handlers.add(handler);
  let active = true;
  return function unlisten() {
    if (!active) return;
    active = false;
    handlers.delete(handler);
    if (handlers.size === 0) {
      addon.stop();
      listening = false;
    }
  };
}

module.exports = { listen };
