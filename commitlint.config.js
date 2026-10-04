module.exports = {
  extends: ["@commitlint/config-conventional"],
  rules: {
    // Allow a leading capital so subjects can start with a filename or proper
    // noun (for example "README for the 1.0 API"). Title case and all-caps stay rejected.
    "subject-case": [2, "never", ["start-case", "pascal-case", "upper-case"]],
  },
};
