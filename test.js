// Manual smoke test: listens for 10 seconds, printing every key event.
// Requires the OS permission (see README) and a desktop session; CI does not
// run this file.
const keylogger = require("./src/index");

const unlisten = keylogger.listen((event) => {
  console.log("key event", event);
});

setTimeout(unlisten, 10000);
