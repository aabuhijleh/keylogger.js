"use strict";

const test = require("node:test");
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");

const { docsOnlySkip } = require("../scripts/docs-only");

const root = path.join(__dirname, "..");
const releaserc = JSON.parse(fs.readFileSync(path.join(root, ".releaserc.json"), "utf8"));

const FEAT_BANG =
  "feat!: keylogger.js 1.0.0 — Linux support, listen() API, safe hooks, prebuilds, staged release (#5)";

function pluginConfig(name) {
  const entry = releaserc.plugins.find((plugin) => Array.isArray(plugin) && plugin[0] === name);
  assert.ok(entry, `${name} is configured`);
  return entry[1];
}

test("type!: is a major release and docs is not", async () => {
  const { analyzeCommits } = await import("@semantic-release/commit-analyzer");
  const config = pluginConfig("@semantic-release/commit-analyzer");
  const notes = pluginConfig("@semantic-release/release-notes-generator");
  assert.deepEqual(notes.parserOpts, config.parserOpts);

  const context = (message) => ({
    commits: [{ message, hash: "abc" }],
    cwd: root,
    logger: { log() {} },
  });

  assert.equal(await analyzeCommits(config, context(FEAT_BANG)), "major");
  assert.equal(await analyzeCommits(config, context("feat: add listen()")), "minor");
  assert.equal(await analyzeCommits(config, context("fix: correct the code")), "patch");
  assert.equal(await analyzeCommits(config, context("docs: fix the events table")), null);
  assert.equal(await analyzeCommits(config, context("docs(readme): fix the events table")), null);
  assert.equal(await analyzeCommits(config, context("docs!: note a break")), "major");
});

test("docs commits skip the rebuild", () => {
  assert.equal(docsOnlySkip({ eventName: "pull_request", prTitle: "docs: fix the events table" }), true);
  assert.equal(
    docsOnlySkip({ eventName: "pull_request", prTitle: "docs(readme): fix the events table" }),
    true
  );
  assert.equal(docsOnlySkip({ eventName: "pull_request", prTitle: "docs!: note a break" }), false);
  assert.equal(docsOnlySkip({ eventName: "pull_request", prTitle: "feat!: ship 1.0.0" }), false);
  assert.equal(
    docsOnlySkip({
      eventName: "push",
      pushCommits: [{ message: "docs: fix the events table\n\nThe links were literal." }],
    }),
    true
  );
  assert.equal(
    docsOnlySkip({
      eventName: "push",
      pushCommits: [{ message: "docs: fix the events table" }, { message: "fix: parse breaking commits" }],
    }),
    false
  );
  assert.equal(
    docsOnlySkip({
      eventName: "push",
      pushCommits: Array.from({ length: 20 }, () => ({ message: "docs: page" })),
    }),
    false
  );
  assert.equal(docsOnlySkip({ eventName: "push", pushCommits: null }), false);
});
