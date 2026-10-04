# keylogger.js

A Node.js addon that reports every key press and release in the session. The user must grant the OS permission below. Do not use it for covert capture.

## Install

Requires Node.js 22 or newer. Runs on macOS 11 or newer, Windows 10 or newer, and Linux with libevdev.

```sh
npm install keylogger.js
```

If no prebuilt binary matches your platform, npm compiles from source. Compiling needs a C++ toolchain. Linux also needs the `libevdev` headers.

## Usage

```js
const keylogger = require("keylogger.js");

const unlisten = keylogger.listen((event) => {
  if (event.code === "Space" && event.state === "down" && !event.repeat) {
    console.log("space pressed");
  }
});

unlisten();
```

`listen` returns a function that removes that subscription. The first subscriber starts the OS listener, and the last `unlisten()` stops it.

`listen` throws a `TypeError` if the handler is not a function. It throws an `Error` if the OS listener cannot start, and the message names the permission.

The addon only listens. It does not swallow keys. If a handler throws, other subscribers still receive the event.

## Events

| Field    | Meaning                                                                                                                |
| -------- | ---------------------------------------------------------------------------------------------------------------------- |
| `key`    | `[KeyboardEvent.key](https://www.w3.org/TR/uievents-key/)`. Space is `" "`.                                            |
| `code`   | `[KeyboardEvent.code](https://www.w3.org/TR/uievents-code/)`. The same string on every OS. `""` if the key is unknown. |
| `state`  | `"down"` on press, `"up"` on release.                                                                                  |
| `repeat` | `true` when holding a key repeats `"down"`.                                                                            |

Use `code` for shortcuts. It stays the same across keyboard layouts. Use `key` for the character. macOS and Windows translate `key` with the active layout. On Windows, Shift does not change `key` for printable keys, so Shift+1 is `"1"`. Linux reports the unshifted US value.

## Permissions

On macOS, grant the app Accessibility or Input Monitoring.

Windows needs no extra permission.

On Linux, add the user to the `input` group, then log out and back in.

```sh
sudo usermod -aG input "$USER"
```

## License

MIT. See [LICENSE](LICENSE).
