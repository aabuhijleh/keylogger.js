/*
capture key down and up events

@example
```
import * as keylogger from "keylogger.js";

keylogger.start((key, isKeyUp, keyCode) => {
  console.log("keyboard event", key, isKeyUp, keyCode);
});
```
*/

/**
 * Start listening to keyboard events
 * key: will match KeyboardEvent.key value as listed in this table https://developer.mozilla.org/en-US/docs/Web/API/KeyboardEvent/key/Key_Values
 * isKeyUp: `true` if the key is up, `false` otherwise
 * keyCode: numerical code representing the value of the pressed key
 */
export const start: (callback: (key: string, isKeyUp: boolean, keyCode: number) => void) => void;

/**
 * Stop listening to keyboard events
 */
export const stop: () => void;
