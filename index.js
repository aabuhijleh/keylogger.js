const addon = require("bindings")("push_to_talk");

addon.start((key, isKeyUp) => {
  console.log({ key, direction: isKeyUp ? "up" : "down" });
});

// setTimeout(() => {
//   addon.stop();
// }, 1000);

// module.exports = addon;
