# TODO

Short execution queue only.

Remove finished items when they are done.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Recent checkpointed lane: `CAOL-DEFENDED-CAMP-SIGHT-SMOKE-HARDENING-v0`.

Status: CHECKPOINTED GREEN / AGENT-SIDE PROOF COMPLETE / AWAITING FRAU REVIEW.

Contract: `doc/defended-camp-sight-smoke-hardening-packet-v0-2026-05-05.md`.

Current checkpoint: deterministic/source-path gates plus staged/live currently-sighted bandit watcher and smoke-out rows are green. Latest live receipts: `.userdata/dev-harness/harness_runs/20260505_102525/` (`bandit.scout_stalker_sight_avoid_live`, `feature-path`, `feature_proof=true`, 10/10) and `.userdata/dev-harness/harness_runs/20260505_102629/` (`bandit.scout_stalker_smoked_watcher_live`, `feature-path`, `feature_proof=true`, 11/11).

Next concrete steps:
- Frau/Augerl review the checkpointed proof and claim boundary;
- if accepted, Schani should close or retitle this packet and promote the next specific greenlit lane;
- Andi should not rerun the green smoke/sight rows unless review finds a concrete evidence gap or code/claim changes.

Do not rerun completed eleven-slice debug-stack proof as ritual.
