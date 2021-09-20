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
 *
 * `key`: string matching KeyboardEvent.key value as listed in this table https://developer.mozilla.org/en-US/docs/Web/API/KeyboardEvent/key/Key_Values
 *
 * `isKeyUp`: boolean that will be `true` if the key is released and `false` if it's pressed down
 *
 * `keyCode`: numerical code representing the value of the pressed key
 */
export const start: (callback: (key: string, isKeyUp: boolean, keyCode: number) => void) => void;

/**
 * Stop listening to keyboard events
 */
export const stop: () => void;
