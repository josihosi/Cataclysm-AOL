# External Phase-3 supervisor

Use this reference only for explicit external supervisor start, restart, stop, or diagnosis.

An explicit `supervisor_service.py start` begins a new runtime-ownership epoch. Before launching
the coordinator, the launcher first verifies that the installed policy source, contracts, and
compiled bytecode agree, then atomically deploys that exact set into the workspace. This policy
deployment is part of restart cleanup; do not copy only the skill scripts or only the bytecode.
After deployment, the launcher atomically normalizes the preceding epoch: it abandons every
nonterminal task attempt, releases its worker claim and active attempt ownership, and releases any
unacknowledged coordinator-restart claim held by the dead run while preserving the semantic restart
request. This cleanup is automatic; do not reproduce it with ad hoc SQLite edits.
The normalization epoch also consumes all prior worker-result routing events. Administrative
`restart_normalized` attempts are terminal history, not fresh completion, finding, or abandonment
events for the new coordinator to ingest.

Normalization preserves project truth and durable evidence: `.de67/FS.md`, the work ledger, the
mutation-suggestion ledger, completed attempts, findings, incidents, mutation state, and semantic
restart generations. The fresh coordinator reads those artifacts and may dispatch new attempts for
unfinished ledger work.

The internal fresh-coordinator transition after a mutation review is not an explicit external start.
It runs inside the existing supervisor epoch and must not invoke external-start normalization or
erase ledger state.

The coordinator chooses when to run `scripts/repository_checkpoint.py`. A checkpoint records a
Git snapshot, not task completion or acceptance; unfinished task rows do not prohibit it. The
command preserves its commit/push recovery identity and reports Git failures for repair. The
supervisor does not require a checkpoint at startup, review exit or coordinator continuation.

A due internal review distinguishes a returned worker turn from its unfinished task. Returned
named assignments retain their claims and evidence while clocks retire for review. The fresh
coordinator may resume the same worker with `message`; an active or uncertain turn still prevents
exclusive review. Explicit external starts retain the normalization behavior described above.

Use `supervisor_service.py status` and `stop` for observation and shutdown. A stopped service never
implies that FS or ledger work is complete.
