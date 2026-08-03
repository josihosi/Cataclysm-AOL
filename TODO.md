# TODO

Short execution queue only.

Remove finished items when they are done.
If the queue below stops matching `Plan.md`, fix this file.

## Now

1. Release reservations only when the current operation ID/generation matches; stale cleanup must
   leave newer owners byte-identical.
2. Cover every matching success/abort/death/migration/origin-loss release path, then the remaining Phase-2
   population/readiness, origin-loss, concurrency, and bounded-selection rows.

Deferred, non-blocking release-harness gap: retain the existing Mac shell export, make no more
Keychain retries or blocker messages while Josef is unavailable, and leave final clean-environment
secure-store/API qualification for the later release gate.
