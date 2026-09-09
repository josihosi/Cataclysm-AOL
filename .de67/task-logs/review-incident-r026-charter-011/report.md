Current status: superseded by [successful installed recovery](../review-r026-installed-recovery/report.md). The earlier approval blocker below is historical.

# R-026 charter incident review — candidate awaiting protected-code authority

Current review: `mutation-f18f34928ded49c59bab1058a266804d`, incident task `R-026-living-route-charter-011`, lineage `semantic-surface-cockpit`, supervisor 95294. No coordinator or roster worker was active. The pending owner briefing entry remains intact until promotion. No coordinator was launched and no live restart was requested.

The charter task completed 342.679848 seconds before its deadline. Receipt `8b9d9ca939bd0c57289aba33b1d4f4167808b4a2a40a708097ab136700890152` and `.de67/task-logs/R-026-living-route-charter-011/charter-validation.json` prove charter validation only: the token was unconsumed and no gameplay or whole-claim acceptance occurred. R-026 still had no acceptance when its claim clock expired. This is a valid claim-level miss, not a late charter task. The current generation-4 incident was diagnosed through the existing API; task completion, timestamps and acceptance were preserved.

## Proven recovery defect and proposed repair

Luna traced the current claim/attempt distinction and reproduced recovery on an isolated copy. The legacy `claim_deadline_incidents` row is copied into generation 1 at harness startup even when the exact same task/timestamp already belongs to generation 4. `_pending_deadline_mutations()` then includes both, while normal resolution selects only the highest generation. Resolving generation 4 leaves the false generation-1 gate pending. Incident validation also selects the unreviewed duplicate because its task-based query has no generation ordering. The normal live guard rejected the already diagnosed current incident for this reason; the failure log is retained.

`compatibility-repair.patch` is an isolated candidate, not installed. It prevents creation of the exact duplicate mirror; excludes already-existing exact mirrors from pending routing while retaining their historical bytes; and makes guard diagnosis lookup select the highest generation. Distinct older incidents remain pending. It does not change a deadline, award acceptance, erase an incident, or replace the normal resolution API. The isolated recovery reaches no remaining mutation gate and requests one successor through the normal resolution path. That isolated generation-48 request is not a live request.

The installed normal mutation guard explicitly rejects changes to `scripts/deadline_harness.py` and `scripts/mutation_guard.py`: “Normal method mutation cannot change the hard clock/guard surface”. Owner approval of this exact protected compatibility repair is the remaining boundary. No live SQL workaround or guard bypass was used. After approval, the protected repair can be installed at the current quiet boundary, the briefing change validated against the repaired lookup, and the incident resolved once through the existing micro/macro lifecycle. Preserve the charter and continue the first unmet gameplay route with a fresh evidence-sized claim clock; do not replay preparation or compress gameplay into the expired clock.

## Pending owner briefing refinement

`briefing-candidate.patch` generalizes Sol's existing handoff guidance from uncertain experiments to coding and testing where useful. It keeps FS outcome authority separate from revisable engineering advice; covers design/interfaces/invariants or test premises/observations as selectable examples; synthesizes bounded research; scales detail to uncertainty; and skips added briefing when it adds no value. Workers retain execution judgment, and the existing context-library route is unchanged. Live coordination now refers to current work rather than only an experiment. No template, extra receipt, approval or review stage was added. The emitted coordinator prompt contains the candidate wording. This proves delivery selection only, not future use or savings.

## Validation

- 74 supervisor tests pass for the briefing candidate.
- 94 deadline-harness tests pass for the isolated compatibility candidate.
- The concrete recovery test proves current generation resolution, preserved mirror evidence, no mirror recreation, and continued blocking for a distinct older incident without resolved components.
- The guard suite reports the same 21 failing test cases on baseline and candidate (69 cases total); no new failing case. This is not a full passing guard suite.
- Standalone normal method validation accepts the briefing-only candidate; the unchanged local random-review comparison and exact owner-queue consumption check pass. Protected-source validation rejects the compatibility patch as expected.
- The supplied policy-guard argv ran without a shell and passed with bytecode identical to the active policy. The kernel and policy were not changed.

The first adversarial recovery fixture accidentally retained historical resolved components, so it could not represent an unresolved distinct older incident. That failed test is retained; the corrected isolated fixture removes only its fixture components and passes. Full logs, candidate source, database copies and scripts remain at `.de67/state/review-incident-r026-charter-011/`.

## Sol's owner-decision reports retained

These are coordinator reports, not new approval or independent validation in this review:

- Night-repair receipt `0892479ce584418db6ef41fc3ef4d594d82b16ca97d6dac9fb423f2c3687185f` reports departure, exact-world reload, approach after dawn at minute 9025 and local contact after dawn at 9085. It does not settle the separately reserved local activation/arbitration choice: clear inherited strategic travel at local admission, or exclude exact locally committed hostile IDs from generic routing.
- Physical-fire receipt `bd79c72395d2eeb925ce1bcf5afcd0ffd18ad24926fbadab390d1f4e8c88979c` reports eligible adjacent-OMT smoke observation, normalized camp memory and drive idle, without scout/operation credit.
- Sound receipt `5f554e3152b2b4265d39662cf9a65b47cab7c8a683fe46a8dbca70e4206f0f21` reports recent-check rejection at minute 8280: only 55 of 360 minutes since last_checked 8225, while sound expires at 8400 before cooldown ends at 8585. The owner choice is whether distant sensing shares the cooldown stamp of completed physical investigation. Aggregate drive 347 is not the first causal gate. A timestamp test plus one saved-lead semantic run is the proposed post-decision check; the firearm need not be replayed. Neither choice was made here.

## Accounting

As of 2026-09-09T18:41:05.770Z, exact review-tree usage is 8,462,398 tokens across 67 unique response records: 8,434,717 input (8,091,264 cached) and 27,681 output. Luna followups/retries are included; later closeout/delayed records and unrelated roots are excluded. No measured savings are claimed. The briefing strings grow by 298 characters; the improvement is broader useful guidance, not compression.
