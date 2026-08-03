# TODO

Short execution queue only.

Remove finished items when they are done.
If the queue below stops matching `Plan.md`, fix this file.

## Now

1. Introduce the test-visible single-writer cutover: observer/signal discovery enabled with legacy
   radar disabled, plus a separately isolated legacy-only control.
2. Prove quiet former-radar, avatar-movement, both-faction no-dual-writer, and autonomous discovery
   controls before removing direct-player targeting.
3. Add evidence provenance/age debug output and the bounded Phase-4 aging/save packet after the
   no-radar production path is green.

Deferred, non-blocking release-harness gap: retain the existing Mac shell export, make no more
Keychain retries or blocker messages while Josef is unavailable, and leave final clean-environment
secure-store/API qualification for the later release gate.
