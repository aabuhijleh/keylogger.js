"use strict";

const fs = require("node:fs");

// Matches the PR title checker: optional scope, no breaking "!".
const DOCS_COMMIT = /^docs(\([a-z0-9][a-z0-9._/-]*\))?: /;

// GitHub's push payload includes at most 20 commits. A full page may be
// truncated, so a 20-commit push is not treated as docs-only.
const PUSH_COMMIT_LIMIT = 20;

function subjectOf(message) {
  return String(message ?? "").split(/\r?\n/, 1)[0];
}

function isDocsOnly(subjects) {
  return subjects.length > 0 && subjects.every((subject) => DOCS_COMMIT.test(subject));
}

function subjectsFromEvent({ eventName, prTitle, pushCommits }) {
  if (eventName === "pull_request") {
    return [subjectOf(prTitle)];
  }
  if (!Array.isArray(pushCommits) || pushCommits.length === 0 || pushCommits.length >= PUSH_COMMIT_LIMIT) {
    return [];
  }
  return pushCommits.map((commit) => subjectOf(commit && commit.message));
}

function docsOnlySkip(event) {
  return isDocsOnly(subjectsFromEvent(event));
}

module.exports = { DOCS_COMMIT, isDocsOnly, subjectsFromEvent, docsOnlySkip };

if (require.main === module) {
  const skip = docsOnlySkip({
    eventName: process.env.EVENT_NAME,
    prTitle: process.env.PR_TITLE,
    pushCommits: JSON.parse(process.env.PUSH_COMMITS || "null"),
  });
  if (process.env.GITHUB_OUTPUT) {
    fs.appendFileSync(process.env.GITHUB_OUTPUT, `skip=${skip}\n`);
  }
  console.log(`docs-only=${skip}`);
}
