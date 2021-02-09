const addon = require("bindings")("push_to_talk");

addon.start((keyCode) => {
  console.log("key is pressed:", keyCode);
});

console.log("testing...");
