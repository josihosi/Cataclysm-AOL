# TODO

## Current necessary claim — `T01-DISC` blocked

Checkpoint `be77732d45` adds the bounded read-only complete-pair discriminator and focused H1/H0
controls without assigning paths, moving actors, or changing authoritative world state. The exact
`20260809_074804` H1/H0 result remains unresolved: its preserved save predates the loaded members
4/5 handoff and its artifacts contain no complete-candidate receipt.

Every discriminator or live worker command must redirect raw stdout and stderr to a named
worker-owned artifact file. The worker returns only the fixed-schema DE67 receipt or a requested
named bounded selector; `--compact-stdout` alone does not prove compact output.

Resume only with an existing exact post-handoff save/complete-candidate receipt or explicit new
authority for a read-only live/replay execution or exact-state fixture construction. Do not infer
H0 from the failed selected pair. `T01-SELECT`, geometry changes, and live lifecycle credit remain
closed until the exact discriminator returns H1 or H0.
