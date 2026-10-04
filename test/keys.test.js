"use strict";

const test = require("node:test");
const assert = require("node:assert/strict");

const { table, lookup } = require("../src/keys");

test("Space key value is a single space, not Spacebar", () => {
  const space = table.find((row) => row.code === "Space");
  assert.ok(space, "table has a Space row");
  assert.equal(space.key, " ");
});

test("every row has a non-empty code and key, and at least one platform code", () => {
  for (const row of table) {
    assert.equal(typeof row.code, "string");
    assert.ok(row.code.length > 0, `row ${JSON.stringify(row)} has empty code`);
    assert.equal(typeof row.key, "string");
    assert.ok(row.key.length > 0, `${row.code} has empty key`);
    assert.ok(
      row.mac != null || row.win != null || row.linux != null,
      `${row.code} has no platform code`
    );
  }
});

test("code values are unique and platform codes are unique per platform", () => {
  const codes = new Set();
  const perPlatform = { mac: new Set(), win: new Set(), linux: new Set() };
  for (const row of table) {
    assert.ok(!codes.has(row.code), `duplicate code ${row.code}`);
    codes.add(row.code);
    for (const platform of Object.keys(perPlatform)) {
      const value = row[platform];
      if (value == null) continue;
      const key =
        platform === "win" && typeof value === "object"
          ? `${value.vk}:${value.extended ? 1 : 0}`
          : String(value);
      assert.ok(
        !perPlatform[platform].has(key),
        `duplicate ${platform} code ${key} (${row.code})`
      );
      perPlatform[platform].add(key);
    }
  }
});

test("the same physical key resolves to the same code on every platform", () => {
  // KeyV: macOS kVK_ANSI_V, Win32 'V' virtual key, Linux KEY_V.
  assert.equal(lookup("darwin", 0x09).code, "KeyV");
  assert.equal(lookup("win32", 0x56).code, "KeyV");
  assert.equal(lookup("linux", 47).code, "KeyV");

  // Space on all three.
  assert.equal(lookup("darwin", 0x31).code, "Space");
  assert.equal(lookup("win32", 0x20).code, "Space");
  assert.equal(lookup("linux", 57).code, "Space");

  // Left Shift on all three.
  assert.equal(lookup("darwin", 0x38).code, "ShiftLeft");
  assert.equal(lookup("win32", 0xa0).code, "ShiftLeft");
  assert.equal(lookup("linux", 42).code, "ShiftLeft");
});

test("Windows distinguishes Enter from NumpadEnter with the extended flag", () => {
  assert.equal(lookup("win32", 0x0d, false).code, "Enter");
  assert.equal(lookup("win32", 0x0d, true).code, "NumpadEnter");
});

test("named keys match UI Events key values", () => {
  assert.equal(lookup("linux", 28).key, "Enter");
  assert.equal(lookup("linux", 15).key, "Tab");
  assert.equal(lookup("linux", 14).key, "Backspace");
  assert.equal(lookup("linux", 1).key, "Escape");
  assert.equal(lookup("linux", 108).key, "ArrowDown");
  assert.equal(lookup("darwin", 0x33).key, "Backspace");
  assert.equal(lookup("darwin", 0x75).key, "Delete");
  assert.equal(lookup("win32", 0x14).key, "CapsLock");
});

test("printable keys default to their unshifted US character", () => {
  assert.equal(lookup("darwin", 0x00).key, "a");
  assert.equal(lookup("win32", 0x31).key, "1");
  assert.equal(lookup("linux", 39).key, ";");
  assert.equal(lookup("linux", 78).key, "+"); // NumpadAdd
});

test("unknown native codes return null", () => {
  assert.equal(lookup("darwin", 0xffff), null);
  assert.equal(lookup("win32", 0xffff, false), null);
  assert.equal(lookup("linux", 9999), null);
  assert.equal(lookup("plan9", 1), null);
});

test("macOS Fn and Help are covered", () => {
  assert.deepEqual(lookup("darwin", 0x3f), { code: "Fn", key: "Fn" });
  assert.equal(lookup("darwin", 0x72).key, "Help");
});
