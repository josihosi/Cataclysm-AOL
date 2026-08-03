# TODO

Short execution queue only.

Remove finished items when they are done.
If the queue below stops matching `Plan.md`, fix this file.

## Now

1. Add bounded local pair cohesion: persisted leader, follower radius, rendezvous deadline,
   deterministic leader re-election after a physical death, bounded reroute attempts, then
   coherent abort-return instead of splitting or teleporting.
2. Define target/staging arrival as every surviving stable member assembled within the cohesion
   radius; first-member arrival must not advance the group or resolve bounty.

Deferred, non-blocking release-harness gap: retain the existing Mac shell export, make no more
Keychain retries or blocker messages while Josef is unavailable, and leave final clean-environment
secure-store/API qualification for the later release gate.
