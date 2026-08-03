# TODO

Short execution queue only.

Remove finished items when they are done.
If the queue below stops matching `Plan.md`, fix this file.

## Now

1. Implement the atomic abstract-to-local ownership transaction for one complete surviving pair:
   freeze abstract advancement, increment the handoff generation, snapshot the exact route/member
   state, bind or spawn both members at one plausible entry edge, then activate local ownership.
2. Add rollback and save/load controls for partial bind/spawn failure so neither dual ownership nor
   member loss can survive a failed handoff.

Deferred, non-blocking release-harness gap: retain the existing Mac shell export, make no more
Keychain retries or blocker messages while Josef is unavailable, and leave final clean-environment
secure-store/API qualification for the later release gate.
