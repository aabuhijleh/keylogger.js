# Keylogger.js

A simple Node.js keylogger for Windows, macOS and Linux

## Prerequisites

Make sure you have [`node-gyp`](https://github.com/nodejs/node-gyp#installation) and its dependencies installed

## Install

```
$ npm install keylogger.js
```

## Usage

```ts
import keylogger from "keylogger.js";
// or
// const keylogger = require("keylogger.js");

keylogger.start((key, isKeyUp, keyCode) => {
  console.log("keyboard event", key, isKeyUp, keyCode);
});
```

## Notes

The key value returned with the callback function passed to `keylogger.start` will match the browser's `KeyboardEvent.key` value as listed in [this table](https://developer.mozilla.org/en-US/docs/Web/API/KeyboardEvent/key/Key_Values)
