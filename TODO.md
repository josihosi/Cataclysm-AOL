# TODO

Short execution queue only.

Remove finished items when they are done.
If the queue below stops matching `Plan.md`, fix this file.

## Now

1. Implement symmetric local-to-abstract dematerialization for the complete surviving pair:
   read both stable NPCs, snapshot changed position/HP/cargo/deaths against the exact local cursor,
   then commit an even abstract epoch and resume the persisted route exactly once.
2. Reject partial/missing local reads transactionally and prove save/load plus replay cannot lose,
   duplicate, or double-advance either member.

Deferred, non-blocking release-harness gap: retain the existing Mac shell export, make no more
Keychain retries or blocker messages while Josef is unavailable, and leave final clean-environment
secure-store/API qualification for the later release gate.
