const addon = require("bindings")("push_to_talk");

addon.start((keyCode, isKeyUp) => {
  console.log(`key is pressed: [${keyCode}] ${isKeyUp}`);
});

// setTimeout(() => {
//   addon.stop();
// }, 5000);
