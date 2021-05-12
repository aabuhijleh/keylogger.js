# Keylogger.js

A simple Node.js keylogger for Windows and macOS

## Install

```
$ npm install keylogger.js
```

## Usage

```ts
import keylogger from "keylogger.js";
// or
// const keylogger = require("keylogger.js");

keylogger.start((key, isKeyUp) => {
  console.log("keyboard event", key, isKeyUp);
});
```

## Notes

The key value returned with the callback function passed to `keylogger.start` will match the browser's `KeyboardEvent.key` value as listed in [this table](https://developer.mozilla.org/en-US/docs/Web/API/KeyboardEvent/key/Key_Values)
