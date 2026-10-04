/*
Report keyboard press and release events while another application is
focused.

@example
```
import keylogger = require("keylogger.js");

const unlisten = keylogger.listen((event) => {
  console.log(event.key, event.code, event.state, event.repeat);
});

// later
unlisten();
```
*/

interface KeyEvent {
  /** UI Events KeyboardEvent.key. Space is " ", U+0020. */
  key: string;
  /** UI Events KeyboardEvent.code, the same string on every OS. */
  code: string;
  /** "down" on press, "up" on release. Autorepeat is "down" with repeat: true. */
  state: "down" | "up";
  repeat: boolean;
}

interface Keylogger {
  /**
   * Subscribe to keyboard events. The first subscriber installs the OS
   * listener; removing the last subscription tears it down.
   *
   * Throws a TypeError if `handler` is not a function, and an Error if the
   * OS listener could not be installed (assistive access on macOS, a failed
   * hook on Windows, group `input` on Linux — the message names the
   * permission).
   *
   * @returns A function that removes this subscription. Idempotent.
   */
  listen(handler: (event: KeyEvent) => void): () => void;
}

declare const keylogger: Keylogger;
export = keylogger;
