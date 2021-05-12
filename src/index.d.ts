/*
capture key down and up events

@example
```
import keylogger from "keylogger.js";

keylogger.start((key, isKeyUp) => {
  console.log("keyboard event", key, isKeyUp);
});
```
*/

/**
 * Start listening to keyboard events
 * key: will match KeyboardEvent.key value as listed in this table https://developer.mozilla.org/en-US/docs/Web/API/KeyboardEvent/key/Key_Values
 * isKeyUp: `true` if the key is up, `false` otherwise
 */
export const start: (callback: (key: string, isKeyUp: boolean) => void) => void;

/**
 * Stop listening to keyboard events
 */
export const stop: () => void;
