# TODO

Short execution queue only.

Remove finished items when they are done.
If the queue below stops matching `Plan.md`, fix this file.

## Now

1. Implement first-survivor provisional report/cargo application while the scout slot remains
   reserved until every member returns, dies, or reaches the fixed missing deadline; prove late
   return and replay behavior across save/load.
2. Finish phase transition constraints, then define the separate `hostile_operation` and camp
   decision owner; keep scout and follow-on reservations mutually exclusive.
3. Add the world-resource, bounded supply, dossier/report revision, pruning, and component
   idempotency owners with focused migration/replay tests.
4. Extend the existing empty/normal/saturated byte evidence only as each new Phase-1 component
   becomes real, then checkpoint the complete Phase-1 model.

Deferred, non-blocking release-harness gap: retain the existing Mac shell export, make no more
Keychain retries or blocker messages while Josef is unavailable, and leave final clean-environment
secure-store/API qualification for the later release gate.
