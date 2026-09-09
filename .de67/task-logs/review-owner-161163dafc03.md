# Owner review 161163dafc03 — 2026-09-09

Authority: the current exclusive invocation `mutation-c2e9fa59dcf44ff2be4a7d6bada101ec`, lineage `semantic-surface-cockpit`, the complete one-entry owner queue, and Josef's current follow-up about the gate ID. Earlier owner/reviewer conversations are evidence only. Evidence root: `.de67/state/review-owner-161163dafc03/`.

## Gate ID: useful identity, unnecessary failure boundary

The supplied ID is valid. `gate-before.json` recomputes **161163dafc03** from the current top-level trigger entries. This owner gate is a queue fingerprint checked alongside durable quiescence/lifecycle state, not an approval credential. The supervisor's unresolved-gate check prevents declaring an unfinished review complete. That check remains.

A different use of the ID was obstructive: `codex_app_server_runner.py::review_context` aborted when optional structured gate metadata was missing or malformed, and `mutator_session.py::review_thread_id` rejected a different interrupted review's identity. Neither case let the agent receive the current review instructions. An owner queue can legitimately change between interrupted invocations. The existing exclusive session lock already prevents competing use. The reproduced rejection and missing-field failure are retained in `gate-before.json`; the missing-field class also occurred in historical runner `20260908T220620Z-90965`, as documented by the prior cycle-13 evidence.

Correction: the gate identifies reusable history. Missing/unusable metadata or a different review selects a fresh context; it does not fabricate a gate, inherit owner authority, or resume unrelated history. A matching unfinished review still resumes. The old interrupted recovery handle is retained under `state/mutator-review-contexts/` when displaced, and the owner conversation remains intact. Current lineage/run bindings, exclusive lock, current prompt, model, and supervisor launch/finish ownership remain authoritative. The legacy random lane is excluded from the history key because it does not identify a different review.

The App Server regression captures one fresh `thread/start` and the exact current `turn/start` input for changed, missing and malformed gate metadata. It also proves matching-review resume, owner-thread preservation, retained interrupted context, and lock exclusion. No live reviewer or successor was launched as a test. This repairs the producing/transport transition rather than adding instructions telling an agent to navigate a launch failure it cannot see.

## Owner queue disposition: typed owner wait

The complete pending entry is supported and completed. The delivered coordinator trace `state/runner-runs/20260909T060912Z-30987/events.jsonl` shows closure dispatch, later worker-result ingress, and the retained duplicate-receipt rejection. Advisory `state/agent-mail/mutator/25e1632c03b14b328d4eb6fd715367ec.json` identifies the R-031 obstruction and explicitly grants no A/B decision.

The task's finite provenance review was useful: receipt `33656681a25465607762bc4efcf348ecd0dea6ff183a2a85888b673604254944` establishes the remaining boundary. The systemic problem was that revision 3 already contained the owner decision in required nonempty proof text, while the kernel inferred execution from that text, repeated the inference in the ledger fallback, and also used unscoped ledger keywords. There was no explicit lifecycle representation of the wait. Rewording the same proof to satisfy a transition would have been ceremony.

Correction: closure revisions carry `route_kind=executable|owner_wait`. Ordinary repair/observation compatibility is retained without guessing authority from prose. An explicit owner wait cannot start a new worker; independent claims and independent executable gaps remain routable. Releasing a wait uses a newly received explicit owner decision through the existing `revise-gap` API. No decision is inferred from its description. The active ledger now exposes this state.

Immediate recovery appended **revision 4** of `R031-package-evidence-integrity`, based on completed task `R-031-closure-provenance-disposition-005`. Its description and proof text are unchanged; only its explicit route kind changes. The terminal task was not terminalized again. Result consumption requires the exact lineage, claim, closure epoch, gap, successor revision, basis task and time. The result cannot be consumed by an unrelated revision merely sharing a task reference.

Josef's exact remaining choice is unchanged: **A**, accept the bounded camp-craft result with its explicit historical source-binding limitation; or **B**, authorize fresh source-bound evidence. Historical expected bytes `ff285f904d73126f5ec052fc5b16237612c7ce158e08dfd95dd2424a3d839c50` remain unavailable; the preserved legacy artifact hashes `785edad70809f8dfbe5c5b67fb491f68b96d370872764b33ff32a41f781fd72b`. This review chooses neither option and performs no gameplay, build or evidence replay.

## Validation and preservation

- `replica-result.json`: a copy of the actual state changes from `receive_worker_result` to `dispatch_exploration_worker` after the exact typed successor, with the original task unchanged.
- Eight focused counterexamples cover nonempty owner waits, repair/observation execution, retired-clock fallback, independent claims, independent same-claim gaps, reused gap labels in different claims, wrong successor identity/time, and explicit decision recovery.
- **255 focused tests pass** across owner-wait, policy kernel, deadline lifecycle, App Server transport and supervisor suites. Four policy-comparison tests also run: two pass and two retain their existing skips. Complete logs and exit statuses are retained.
- The broader mutation-guard suite has the same 21 failure/error identities on the untouched baseline and candidate. They concern existing cadence and legacy guideline fixtures; `baseline-failure-comparison.json` records equality. No all-suite-green claim is made, and unrelated fixture repairs are not prerequisites for the supported corrections.
- `method-guard.json` validates the exact nine changed method paths. The owner entry explicitly authorizes the necessary protected lifecycle edit; the broader method-candidate validator is used only for that scope and current gate-context correction, not as unrelated universal authority. No local guideline or FS outcome changed.
- `policy-guard-prepromotion.json` and `supplied-policy-guard.log` validate the extended 29-case decision corpus and existing temporal corpus. The supplied guard argument array was executed without a shell. Active policy source/bytecode remain unchanged; the machine fact extraction and contract examples change.
- `ledger-guard.log` validates the additive ledger handoff. Existing assignment identities, independent assignments, subdivisions and proof remain intact.
- `durable-preservation.json` compares every existing database table against the initial snapshot: the sole data difference before restart request is one appended closure revision. Existing proof columns, receipts, terminal tasks, accepted claims, clocks and terminal-window counts are unchanged. `promotion.json` binds installed bytes.

Stop condition: supported corrections are reproduced, installed and validated. The live policy still honors the pending review gate; after removing only review/restart facts, its independently actionable projection is exploration dispatch. That projection is not a claim that a successor has already run. The next naturally authorized runner loads the installed transport; no service restart or competing coordinator is manufactured for proof.

## Accounting and closeout

`accounting-tree-asof.json` deduplicates own-response records for this exact reviewer turn and its Luna descendant, including the helper's corrective follow-ups. At the retained 06:41:37–06:41:55 UTC source timestamps: **7,551,350 total tokens**, comprising 7,514,852 input (7,254,656 cached) and 36,498 output. Astra uses 5,174,800 total; Luna uses 2,376,550. Uncached input plus output is 296,694. Reasoning is a subset of output. Discovered-tree coverage has no unavailable IDs, source errors, partial or conflicting records. Later closeout/telemetry writes are outside this as-of total. The earlier root-only prefix was incomplete and is superseded by this tree extraction. These are telemetry counts, not billing.

No comparable before/after cost experiment was performed, so no measured token reduction is claimed. Expected savings are avoiding failed review launches and repeated owner-wait routing; helper, retry and validation costs of this review remain included in the observed tree rather than hidden.

The completed owner entry is removed from the consumable queue. Durable artifacts retain its exact text and disposition. `resolution.json` records one fresh coordinator restart request and its before/after count. The external supervisor alone may launch/acknowledge that successor; any owner stop remains in force. The successor should continue independently actionable claims while preserving R-031's owner-only A/B boundary.
