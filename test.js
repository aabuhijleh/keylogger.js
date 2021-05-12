const keylogger = require("./src/index");

keylogger.start((key, isKeyUp) => {
  console.log("key event", key, isKeyUp);
});

setTimeout(() => {
  keylogger.stop();
}, 10000);
