# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Active target: `Cannibal camp night-raid behavior packet v0` live/harness product-proof slice.

Current state: deterministic/code substrate is green, and the first live matrix slice has real feature-path evidence under proof-freeze rules. Green runs:

- Day smoke/light pressure: `.userdata/dev-harness/harness_runs/20260428_124902/` — smoke lead, cannibal `active_job=stalk`, `posture=hold_off`, no shakedown/combat-forward.
- Night local-contact attack window: `.userdata/dev-harness/harness_runs/20260428_124947/` — direct/local contact, `darkness_or_concealment=yes`, `pack_size=2`, `posture=attack_now`, `combat_forward=yes`, `shakedown=no`.
- Exposed/recent-sight hold-off: `.userdata/dev-harness/harness_runs/20260428_125138/` — bounded 20-turn live window, cannibal `sight_exposure=recent`, `posture=hold_off`, no shakedown/combat-forward.
- Fixture-bias smoke: `repeatability --count 2 cannibal.live_world_day_smoke_pressure_mcw`, stable pass in `.userdata/dev-harness/harness_runs/20260428_125319/` and `.userdata/dev-harness/harness_runs/20260428_125342/`.
- Save/writeback persistence: `.userdata/dev-harness/harness_runs/20260428_130948/` — day-smoke creates active cannibal `stalk` group, guarded save advances player-save mtime, saved `dimension_data.gsav` contains `profile=cannibal_camp`, active group/target, `active_member_ids=[4,5]`, and `known_recent_marks` with `live_smoke@...`; `intelligence_map.leads=[]` is explicitly out of scope, not credited.
- No-fixture reload support: `.userdata/dev-harness/harness_runs/20260428_131031/` — fresh startup without fixture reinstall plus metadata audit still sees the active cannibal group/profile/target/member state and `known_recent_marks` smoke signal.

Playtest matrix: `doc/cannibal-camp-night-raid-live-playtest-matrix-v0-2026-04-28.md`.

Next audited scenarios:

1. Explicit daylight/no-cover + high-threat negative if Schani wants this separated from the day-smoke hold-off proof.
2. Optional bandit-control contrast for shakedown/pay/fight, if product review wants a labeled control beyond the cannibal `shakedown=no` contact proof.

Proof discipline:

- Every setup/action/wait/input needs a step-ledger row, screenshot/OCR or exact metadata/log proof, failure rule, and artifact path.
- Load-and-close is startup proof only; deterministic evaluator output is support only; no product closure from hidden setup or red prerequisites.
- Stop after two same-blocker attempts and consult Frau Knackal before attempt three; after four unresolved attempts, package the implemented-but-unproven result for Josef instead of looping.

Remaining boundary:

- Do not claim full live night-raid closure until the remaining optional negative/control proof is either green or explicitly scoped out; scenarios already marked green have feature-path evidence, and scenario 6 is saved-state persistence plus reload support, not magical in-memory proof.
- Do not widen into cannibal lore/capture mechanics/unrelated bandit rewrites while chasing playtest coverage.

Keep this file focused on the active lane only, ja, otherwise it turns into a junk drawer with headings.
