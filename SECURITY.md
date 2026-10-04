# Security

`keylogger.js` observes every key press and release in the user's session, by
design, after the user grants the OS permission. A bug in the listener or in
the build/release pipeline is therefore security-sensitive.

## Reporting a vulnerability

Please do not open a public issue for a security problem. Report it through
[GitHub's private vulnerability reporting](https://github.com/aabuhijleh/keylogger.js/security/advisories/new)
for this repository. Include the platform, Node version, and a way to
reproduce the behavior.

## Scope

In scope: the native listener on all three platforms, the JavaScript wrapper,
and the release workflow (prebuilds, staged npm publish). Out of scope:
applications that use this package to capture input without the user's
knowledge — that is a prohibited use, not a vulnerability in the package.
