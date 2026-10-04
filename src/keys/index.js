"use strict";

// One row per physical key. `code` and `key` follow the UI Events
// KeyboardEvent.code / KeyboardEvent.key specifications; `key` is the
// unshifted US-layout value and native layout translation overrides it for
// printable keys at runtime. Platform columns hold the native code:
//   mac:   CGKeyCode / Carbon kVK_*
//   win:   Win32 virtual-key code, or { vk, extended } when the extended
//          flag is needed (Enter vs NumpadEnter)
//   linux: KEY_* from linux/input-event-codes.h
// null means the platform does not produce that key.
const table = [
  // Letters (positions follow the US QWERTY layout).
  { code: "KeyA", key: "a", mac: 0x00, win: 0x41, linux: 30 },
  { code: "KeyB", key: "b", mac: 0x0b, win: 0x42, linux: 48 },
  { code: "KeyC", key: "c", mac: 0x08, win: 0x43, linux: 46 },
  { code: "KeyD", key: "d", mac: 0x02, win: 0x44, linux: 32 },
  { code: "KeyE", key: "e", mac: 0x0e, win: 0x45, linux: 18 },
  { code: "KeyF", key: "f", mac: 0x03, win: 0x46, linux: 33 },
  { code: "KeyG", key: "g", mac: 0x05, win: 0x47, linux: 34 },
  { code: "KeyH", key: "h", mac: 0x04, win: 0x48, linux: 35 },
  { code: "KeyI", key: "i", mac: 0x22, win: 0x49, linux: 23 },
  { code: "KeyJ", key: "j", mac: 0x26, win: 0x4a, linux: 36 },
  { code: "KeyK", key: "k", mac: 0x28, win: 0x4b, linux: 37 },
  { code: "KeyL", key: "l", mac: 0x25, win: 0x4c, linux: 38 },
  { code: "KeyM", key: "m", mac: 0x2e, win: 0x4d, linux: 50 },
  { code: "KeyN", key: "n", mac: 0x2d, win: 0x4e, linux: 49 },
  { code: "KeyO", key: "o", mac: 0x1f, win: 0x4f, linux: 24 },
  { code: "KeyP", key: "p", mac: 0x23, win: 0x50, linux: 25 },
  { code: "KeyQ", key: "q", mac: 0x0c, win: 0x51, linux: 16 },
  { code: "KeyR", key: "r", mac: 0x0f, win: 0x52, linux: 19 },
  { code: "KeyS", key: "s", mac: 0x01, win: 0x53, linux: 31 },
  { code: "KeyT", key: "t", mac: 0x11, win: 0x54, linux: 20 },
  { code: "KeyU", key: "u", mac: 0x20, win: 0x55, linux: 22 },
  { code: "KeyV", key: "v", mac: 0x09, win: 0x56, linux: 47 },
  { code: "KeyW", key: "w", mac: 0x0d, win: 0x57, linux: 17 },
  { code: "KeyX", key: "x", mac: 0x07, win: 0x58, linux: 45 },
  { code: "KeyY", key: "y", mac: 0x10, win: 0x59, linux: 21 },
  { code: "KeyZ", key: "z", mac: 0x06, win: 0x5a, linux: 44 },

  // Digit row.
  { code: "Digit0", key: "0", mac: 0x1d, win: 0x30, linux: 11 },
  { code: "Digit1", key: "1", mac: 0x12, win: 0x31, linux: 2 },
  { code: "Digit2", key: "2", mac: 0x13, win: 0x32, linux: 3 },
  { code: "Digit3", key: "3", mac: 0x14, win: 0x33, linux: 4 },
  { code: "Digit4", key: "4", mac: 0x15, win: 0x34, linux: 5 },
  { code: "Digit5", key: "5", mac: 0x17, win: 0x35, linux: 6 },
  { code: "Digit6", key: "6", mac: 0x16, win: 0x36, linux: 7 },
  { code: "Digit7", key: "7", mac: 0x1a, win: 0x37, linux: 8 },
  { code: "Digit8", key: "8", mac: 0x1c, win: 0x38, linux: 9 },
  { code: "Digit9", key: "9", mac: 0x19, win: 0x39, linux: 10 },

  // Function keys.
  { code: "F1", key: "F1", mac: 0x7a, win: 0x70, linux: 59 },
  { code: "F2", key: "F2", mac: 0x78, win: 0x71, linux: 60 },
  { code: "F3", key: "F3", mac: 0x63, win: 0x72, linux: 61 },
  { code: "F4", key: "F4", mac: 0x76, win: 0x73, linux: 62 },
  { code: "F5", key: "F5", mac: 0x60, win: 0x74, linux: 63 },
  { code: "F6", key: "F6", mac: 0x61, win: 0x75, linux: 64 },
  { code: "F7", key: "F7", mac: 0x62, win: 0x76, linux: 65 },
  { code: "F8", key: "F8", mac: 0x64, win: 0x77, linux: 66 },
  { code: "F9", key: "F9", mac: 0x65, win: 0x78, linux: 67 },
  { code: "F10", key: "F10", mac: 0x6d, win: 0x79, linux: 68 },
  { code: "F11", key: "F11", mac: 0x67, win: 0x7a, linux: 87 },
  { code: "F12", key: "F12", mac: 0x6f, win: 0x7b, linux: 88 },
  { code: "F13", key: "F13", mac: 0x69, win: 0x7c, linux: 183 },
  { code: "F14", key: "F14", mac: 0x6b, win: 0x7d, linux: 184 },
  { code: "F15", key: "F15", mac: 0x71, win: 0x7e, linux: 185 },
  { code: "F16", key: "F16", mac: 0x6a, win: 0x7f, linux: 186 },
  { code: "F17", key: "F17", mac: 0x40, win: 0x80, linux: 187 },
  { code: "F18", key: "F18", mac: 0x4f, win: 0x81, linux: 188 },
  { code: "F19", key: "F19", mac: 0x50, win: 0x82, linux: 189 },
  { code: "F20", key: "F20", mac: 0x5a, win: 0x83, linux: 190 },

  // Punctuation (US layout).
  { code: "Minus", key: "-", mac: 0x1b, win: 0xbd, linux: 12 },
  { code: "Equal", key: "=", mac: 0x18, win: 0xbb, linux: 13 },
  { code: "BracketLeft", key: "[", mac: 0x21, win: 0xdb, linux: 26 },
  { code: "BracketRight", key: "]", mac: 0x1e, win: 0xdd, linux: 27 },
  { code: "Backslash", key: "\\", mac: 0x2a, win: 0xdc, linux: 43 },
  { code: "Semicolon", key: ";", mac: 0x29, win: 0xba, linux: 39 },
  { code: "Quote", key: "'", mac: 0x27, win: 0xde, linux: 40 },
  { code: "Backquote", key: "`", mac: 0x32, win: 0xc0, linux: 41 },
  { code: "Comma", key: ",", mac: 0x2b, win: 0xbc, linux: 51 },
  { code: "Period", key: ".", mac: 0x2f, win: 0xbe, linux: 52 },
  { code: "Slash", key: "/", mac: 0x2c, win: 0xbf, linux: 53 },
  { code: "IntlBackslash", key: "\\", mac: 0x0a, win: 0xe2, linux: 86 },

  // Whitespace and editing.
  { code: "Enter", key: "Enter", mac: 0x24, win: { vk: 0x0d, extended: false }, linux: 28 },
  { code: "Tab", key: "Tab", mac: 0x30, win: 0x09, linux: 15 },
  { code: "Space", key: " ", mac: 0x31, win: 0x20, linux: 57 },
  { code: "Backspace", key: "Backspace", mac: 0x33, win: 0x08, linux: 14 },
  { code: "Escape", key: "Escape", mac: 0x35, win: 0x1b, linux: 1 },
  { code: "Delete", key: "Delete", mac: 0x75, win: 0x2e, linux: 111 },
  { code: "Insert", key: "Insert", mac: null, win: 0x2d, linux: 110 },
  { code: "Home", key: "Home", mac: 0x73, win: 0x24, linux: 102 },
  { code: "End", key: "End", mac: 0x77, win: 0x23, linux: 107 },
  { code: "PageUp", key: "PageUp", mac: 0x74, win: 0x21, linux: 104 },
  { code: "PageDown", key: "PageDown", mac: 0x79, win: 0x22, linux: 109 },

  // Arrows.
  { code: "ArrowUp", key: "ArrowUp", mac: 0x7e, win: 0x26, linux: 103 },
  { code: "ArrowDown", key: "ArrowDown", mac: 0x7d, win: 0x28, linux: 108 },
  { code: "ArrowLeft", key: "ArrowLeft", mac: 0x7b, win: 0x25, linux: 105 },
  { code: "ArrowRight", key: "ArrowRight", mac: 0x7c, win: 0x27, linux: 106 },

  // Modifiers.
  { code: "ControlLeft", key: "Control", mac: 0x3b, win: 0xa2, linux: 29 },
  { code: "ControlRight", key: "Control", mac: 0x3e, win: 0xa3, linux: 97 },
  { code: "ShiftLeft", key: "Shift", mac: 0x38, win: 0xa0, linux: 42 },
  { code: "ShiftRight", key: "Shift", mac: 0x3c, win: 0xa1, linux: 54 },
  { code: "AltLeft", key: "Alt", mac: 0x3a, win: 0xa4, linux: 56 },
  { code: "AltRight", key: "Alt", mac: 0x3d, win: 0xa5, linux: 100 },
  { code: "MetaLeft", key: "Meta", mac: 0x37, win: 0x5b, linux: 125 },
  { code: "MetaRight", key: "Meta", mac: 0x36, win: 0x5c, linux: 126 },

  // Locks and system keys.
  { code: "CapsLock", key: "CapsLock", mac: 0x39, win: 0x14, linux: 58 },
  { code: "NumLock", key: "NumLock", mac: null, win: 0x90, linux: 69 },
  { code: "ScrollLock", key: "ScrollLock", mac: null, win: 0x91, linux: 70 },
  { code: "PrintScreen", key: "PrintScreen", mac: null, win: 0x2c, linux: 99 },
  { code: "Pause", key: "Pause", mac: null, win: 0x13, linux: 119 },
  { code: "ContextMenu", key: "ContextMenu", mac: null, win: 0x5d, linux: 139 },
  { code: "Fn", key: "Fn", mac: 0x3f, win: null, linux: null },
  { code: "Help", key: "Help", mac: 0x72, win: null, linux: 138 },
  { code: "Clear", key: "Clear", mac: 0x47, win: null, linux: null },

  // Numpad.
  { code: "Numpad0", key: "0", mac: 0x52, win: 0x60, linux: 82 },
  { code: "Numpad1", key: "1", mac: 0x53, win: 0x61, linux: 79 },
  { code: "Numpad2", key: "2", mac: 0x54, win: 0x62, linux: 80 },
  { code: "Numpad3", key: "3", mac: 0x55, win: 0x63, linux: 81 },
  { code: "Numpad4", key: "4", mac: 0x56, win: 0x64, linux: 75 },
  { code: "Numpad5", key: "5", mac: 0x57, win: 0x65, linux: 76 },
  { code: "Numpad6", key: "6", mac: 0x58, win: 0x66, linux: 77 },
  { code: "Numpad7", key: "7", mac: 0x59, win: 0x67, linux: 71 },
  { code: "Numpad8", key: "8", mac: 0x5b, win: 0x68, linux: 72 },
  { code: "Numpad9", key: "9", mac: 0x5c, win: 0x69, linux: 73 },
  { code: "NumpadAdd", key: "+", mac: 0x45, win: 0x6b, linux: 78 },
  { code: "NumpadSubtract", key: "-", mac: 0x4e, win: 0x6d, linux: 74 },
  { code: "NumpadMultiply", key: "*", mac: 0x43, win: 0x6a, linux: 55 },
  { code: "NumpadDivide", key: "/", mac: 0x4b, win: 0x6f, linux: 98 },
  { code: "NumpadDecimal", key: ".", mac: 0x41, win: 0x6e, linux: 83 },
  {
    code: "NumpadEnter",
    key: "Enter",
    mac: 0x4c,
    win: { vk: 0x0d, extended: true },
    linux: 96,
  },
  { code: "NumpadEqual", key: "=", mac: 0x51, win: null, linux: 117 },
  { code: "NumpadComma", key: ",", mac: 0x5f, win: null, linux: 121 },
];

// Per-platform maps from native code to { code, key }, built once.
const maps = {
  darwin: new Map(),
  win32: new Map(),
  linux: new Map(),
};

// Windows virtual keys collide between Enter and NumpadEnter, so the Win32
// map is keyed by vk * 2 + extended.
for (const row of table) {
  const entry = { code: row.code, key: row.key };
  if (row.mac != null) maps.darwin.set(row.mac, entry);
  if (row.linux != null) maps.linux.set(row.linux, entry);
  if (row.win != null) {
    const vk = typeof row.win === "object" ? row.win.vk : row.win;
    const extended = typeof row.win === "object" && row.win.extended;
    maps.win32.set(vk * 2 + (extended ? 1 : 0), entry);
  }
}

/**
 * Resolve a native key code to its UI Events `{ code, key }` pair.
 *
 * @param {"darwin"|"win32"|"linux"} platform process.platform value
 * @param {number} nativeCode CGKeyCode, Win32 virtual-key code, or Linux KEY_*
 * @param {boolean} [extended] Win32 extended-key flag (ignored elsewhere)
 * @returns {{ code: string, key: string } | null}
 */
function lookup(platform, nativeCode, extended) {
  const map = maps[platform];
  if (!map) return null;
  if (platform === "win32") {
    return map.get(nativeCode * 2 + (extended ? 1 : 0)) || map.get(nativeCode * 2) || null;
  }
  return map.get(nativeCode) || null;
}

module.exports = { table, lookup };
