# TODO

Short execution queue only.

Remove finished items when they are done.
If the queue below stops matching `Plan.md`, fix this file.

## Now

1. Independently audit the existing serialized `bandit_live_world` schema, current authority
   overlaps, missing-field behavior, and legacy migration seams.
2. Select the smallest Phase-1 authoritative-state slice without introducing a second competing
   model or behavior change.
3. Add focused missing/new-field, legacy migration, round-trip, and malformed-packet atomicity
   tests for that slice.
4. Measure empty/normal/saturated serialized bytes against the ratified caps and create a narrow
   behavior/model checkpoint.

Deferred, non-blocking release-harness gap: retain the existing Mac shell export, make no more
Keychain retries or blocker messages while Josef is unavailable, and leave final clean-environment
secure-store/API qualification for the later release gate.
