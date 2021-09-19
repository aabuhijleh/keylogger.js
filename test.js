const keylogger = require("./src/index");

keylogger.start((key, isKeyUp, keyCode) => {
  console.log("key event", key, isKeyUp, keyCode);
});

setTimeout(() => {
  keylogger.stop();
}, 10000);
