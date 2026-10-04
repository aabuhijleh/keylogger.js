/*
Report keyboard press and release events while another application is
focused, for example for push-to-talk.

@example
```
import keylogger = require("keylogger.js");

keylogger.start((event) => {
  if (event.code === "Space" && event.state === "down" && !event.repeat) {
    // push-to-talk pressed
  }
});
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
   * Start listening for keyboard events. Throws a TypeError if `callback` is
   * not a function, and an Error if already listening or if the OS listener
   * could not be installed (assistive access on macOS, a failed hook on
   * Windows, group `input` on Linux — the message names the permission).
   */
  start(callback: (event: KeyEvent) => void): void;
  /** Stop listening and release the OS hook. Safe to call when not listening. */
  stop(): void;
}

declare const keylogger: Keylogger;
export = keylogger;
