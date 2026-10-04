# Keylogger.js

Native addon that reports keyboard press and release so an app can act on them, for example push-to-talk.

## Public API (1.0)

CommonJS: `const keylogger = require("keylogger.js")`.

- `keylogger.listen(handler)` — `handler` receives one `KeyEvent` object: `{ key, code, state, repeat }`.
  - `key`: UI Events `KeyboardEvent.key` (Space is `" "`). Layout-translated on macOS/Windows; unshifted US value on Linux.
  - `code`: UI Events `KeyboardEvent.code`, the same string on every OS. Use it for shortcuts.
  - `state`: `"down"` | `"up"`; autorepeat is `"down"` with `repeat: true`.
- `listen` returns an idempotent `unlisten()`. Multiple subscribers are supported: the wrapper fans events out, the first subscriber installs the OS listener, and the last unsubscribe tears it down and frees native resources.
- `listen` throws a `TypeError` for a non-function handler and an `Error` naming the permission if the OS listener cannot be installed (macOS assistive access, Windows hook failure, Linux group `input`). Handler exceptions are swallowed so they cannot starve other subscribers.

Types: `src/index.d.ts` uses `export =` (the runtime is CommonJS). `package.json` has an `exports` map with `types` first.

## Layout of the code

- `src/index.js` — the public wrapper: argument checks, key translation, subscriber fan-out, lazy addon load. The addon is a thin binding with a single dispatch; multiplexing lives in the wrapper.
- `src/keys/index.js` — one shared key table: per physical key, the UI Events `code`/`key` plus the macOS (CGKeyCode), Windows (VK), and Linux (KEY_*) native codes. This table is the single source of truth; do not add per-platform tables.
- `src/macOS/keylogger.mm`, `src/windows/keylogger.cc`, `src/linux/keylogger.cc` — one backend per OS (CGEventTap / WH_KEYBOARD_LL / libevdev read of `/dev/input/event*`, never `EVIOCGRAB`).
- The addon calls the JS dispatch with a raw event `{ keyCode, extended, state, repeat, character }`; the wrapper maps it through the key table into a `KeyEvent`.
- OS callbacks must return immediately: copy the event, `tsfn.NonBlockingCall` (bounded queue, 1024; drops are counted), always pass the event through. `start` throws synchronously on install failure (worker thread reports via `std::promise`/`std::future`).

## Tests and builds

- `npm test` — node:test unit tests for the key table and the wrapper contract. Runs on any OS without a desktop session.
- `test.js` — manual 10-second listener; needs a desktop session and the OS permission. CI must not run it.
- `npm run prebuild` — `prebuildify --napi --strip` into `prebuilds/` (gitignored, packed into the npm tarball).
- Toolchain: node-addon-api 8 (C++17, `NAPI_VERSION=8`), Node `>=22`, loader `node-gyp-build`, macOS deployment target 11.0.
- Local Linux builds need libevdev headers; CI installs `libevdev-dev` on ubuntu runners.

## Releases

Versions, tags, and release notes are owned by semantic-release from Conventional Commits on `main` (`.releaserc.json`); `package.json` stays at `0.0.0-development`. Publishing uses npm staged publishing with trusted publishing (OIDC) from `.github/workflows/ci.yml`: CI stages the package (`npm stage publish`), and a maintainer approves the stage with 2FA on npmjs.com. Never hand-bump the version, never tag `v0.0.4`-style versions by hand, and never run `npm publish` directly. Pull requests are squash-merged and the PR title is the commit subject on `main`. CI checks that title (`.github/workflows/pr-title.yml`); `npm install` installs lefthook: `commit-msg` runs commitlint, and `pre-commit` checks clang-format on staged native sources. Releasable commits are `feat`, `fix`, and `perf`. A `!` after the type is a major release. `docs` and `docs(scope)` commits skip the build, prebuild, and release jobs.
