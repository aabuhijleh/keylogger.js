/*
listen to key down and up events
@example
```
import pushToTalk from "push-to-talk";

// start listening to keyboard events
pushToTalk.start((key: string, isKeyUp: boolean) => {
    console.log({ key, direction: isKeyUp ? "up" : "down" });
});
```
*/

/**
 * Start listening to keyboard events
 */
export const start: (callback: (key: string, isKeyUp: boolean) => void) => void;

/**
 * Stop listening to keyboard events
 */
export const stop: () => void;
