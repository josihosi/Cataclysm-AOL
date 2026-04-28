# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Active target: `C-AOL actual playtest verification stack v0` / `Cannibal camp confidence-push live playtest packet v0`.

Current state: the generic clean-code boundary review is checkpointed. The active cannibal confidence-push matrix now exists at `doc/cannibal-camp-confidence-push-live-playtest-matrix-v0-2026-04-29.md`. The stale runtime/window blocker from the first bandit contrast attempt is cleared: after commit `7ca870f6be`, `make -j4 TILES=1 cataclysm-tiles LINTJSON=0 ASTYLE=` rebuilt the current tiles binary, no leftover game process remained, and `.userdata/dev-harness/harness_runs/20260429_012915/` captured window/runtime `7ca870f6be` with `version_matches_repo_head=true`. The bandit contrast control is green, and cannibal day smoke/pressure is green at `.userdata/dev-harness/harness_runs/20260429_013310/`; cannibal night/contact, reload, and different-footing rows remain open.

Canonical anchors for the active target:

- Stack contract: `doc/c-aol-actual-playtest-verification-stack-v0-2026-04-27.md`.
- Cannibal confidence contract: `doc/cannibal-camp-confidence-push-live-playtest-packet-v0-2026-04-28.md`.
- Cannibal confidence imagination source: `doc/cannibal-camp-confidence-push-live-playtest-imagination-source-of-truth-2026-04-28.md`.
- Existing matrix support: `doc/cannibal-camp-night-raid-live-playtest-matrix-v0-2026-04-28.md`.
- Active confidence matrix: `doc/cannibal-camp-confidence-push-live-playtest-matrix-v0-2026-04-29.md`.

Next narrow work queue:

1. Run one bounded night/contact proof: `cannibal.live_world_night_local_contact_pack_mcw`.
2. Then continue reload-brain proof with the paired persistence/reload scenarios, or select a different-footing repeat if a suitable packaged footing exists.
3. Update the confidence matrix/verdict only from all-green step-ledger + claim-scoped artifact evidence; startup/load or version-mismatch runs stay non-green support.

Proof discipline:

- This is confidence uplift for already-closed cannibal behavior, not a redesign lane.
- Preserve evidence class boundaries: live feature proof, deterministic support, startup/load, artifacts/logs, and Josef/manual evidence are separate.
- Do not promote player-lit fire or roof-horde work while the fuel/writeback gate remains blocked.
