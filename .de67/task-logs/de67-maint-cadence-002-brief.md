# DE67-MAINT-CADENCE-002

## Outcome

Implement and verify the owner-authorized temporary periodic-review cadence: each new cycle uses an inclusive random interval of 10–20 completed worker task attempts, the existing current cycle adopts that range without resetting already-counted progress, and the next review is a general evidence-led search for useful improvements. This machinery work must not alter product claim acceptance or interrupt active workers.

## Authority and current state

- Workspace projection: `/Volumes/CodexBulk/Schanigarten/workspaces/Cataclysm-AOL-hostile-ecology-dev/.de67/work-ledger.md`, `DE67-MAINT-CADENCE`.
- Named non-product binding: `.de67/DFS.md`, `DE67-MAINT-CADENCE-S001`.
- Installed implementation scope: `/Users/josefhorvath/.codex/skills/de67/de-67-3/scripts/deadline_harness.py`, its focused tests, reviewer prompt/configuration, and the live SQLite state `/Volumes/CodexBulk/Schanigarten/workspaces/Cataclysm-AOL-hostile-ecology-dev/.de67/state/deadlines.sqlite3` through deadline-harness transitions only.
- Current lineage is `semantic-surface-cockpit`. Re-read the current random-mutation cycle before changing it. The latest observed state was cycle 9, interval 36, due count 282, and completed terminal windows 277. Historical handoff numbers are not authority if current state differs.
- Preserve elapsed progress from the current cycle start. Do not reset the count, duplicate a terminal-attempt increment, restart the coordinator, or interrupt workers.
- The reviewer prompt was already changed to choose a general inquiry from recent evidence. Verify that behavior; do not revive the old special DFS/universal trigger or treat legacy random-lane metadata as the review target. Preserve historical review records.

## Required implementation and proof

- Change subsequent cadence sampling from the existing 20–50 range to inclusive 10–20.
- Apply the shorter interval to the pending current cycle while retaining its original start/completed count. If elapsed attempts already meet or exceed the chosen interval, expose the next review as due; otherwise report the exact next due completed-attempt count.
- Use supported deadline-harness machinery or a tested migration/command for live SQLite mutation. Do not edit SQLite ad hoc.
- Prove inclusive lower/upper boundaries, deterministic or patched-random selection behavior, exactly-once counting per terminal worker attempt, persisted cycle state across reopen/restart observation, preservation of active tasks and completed work, and the actual updated current cycle.
- Run the smallest relevant full focused suite, not only a one-off assertion. Capture complete logs under `build_logs/` or `.de67/task-logs/`.
- A coordinator-side preliminary edge repair is already present in `scripts/policy_kernel.py` and `tests/test_policy_kernel.py`: unclaimed supervisor-abandoned tasks no longer masquerade as pending worker results. Treat those files as yours for this assignment, validate or improve the patch, and include it in the receipt. Its focused test passed once. This repair is part of making cadence routing advance; do not broaden it to claimed worker returns.

## Coordination and ownership

- You exclusively own the installed DE67 script/test edits for this assignment. Preserve unrelated workspace changes.
- Do not mutate product DFS status, WEC, work ledger, claim acceptances, coordinator restart state, or supervisor processes. Do not access `.de67/no-go-zone/`.
- Send concise progress/questions to `/root`. Continue through ordinary implementation or test failures when a supported repair remains.

## Return

When the whole assignment exits, write a schema-valid identity-bound worker receipt to `.de67/task-logs/de67-maint-cadence-002-worker-receipt.json`. Include exact code/test changes, live state transition command and before/after cycle values, evidence limits, preserved work/active tasks, artifact hashes, no-replay guidance, and the first remaining boundary. Do not terminalize deadline state yourself.
