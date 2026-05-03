# TODO

Short execution queue only.

Remove finished items when they are done.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Active lane: `CAOL-WRITHING-STALKER-THREAT-DISTRACTION-HANDOFF-v0`.

Current execution item: live/staged validation for the writhing-stalker threat/distraction checkpoint.

Contract: `doc/writhing-stalker-threat-distraction-handoff-packet-v0-2026-05-02.md`.

Imagination source: `doc/writhing-stalker-threat-distraction-handoff-imagination-source-2026-05-02.md`.

Handoff packet: `doc/writhing-stalker-threat-distraction-handoff-handoff-v0-2026-05-02.md`.

Current footing:
- Deterministic implementation checkpoint is green: `doc/writhing-stalker-threat-distraction-deterministic-checkpoint-v0-2026-05-03.md`.
- Current-build live/staged proof packet is green: `doc/writhing-stalker-threat-distraction-live-staged-proof-v0-2026-05-03.md`.
- High-threat/allies row: `writhing_stalker.live_high_threat_allied_light_retreat_stalk_mcw` -> `.userdata/dev-harness/harness_runs/20260503_021310/` (`feature_proof=true`, `verdict=artifacts_matched`, green step ledger, portal clear; same-run live_plan line reports `decision=withdraw`, `reason=live_high_threat_allied_light_retreat_stalk`, `allied_support=3`, `persistent=yes`, `stalk_omt=3`).
- Zombie/distraction row: `writhing_stalker.live_zombie_distraction_mcw` -> `.userdata/dev-harness/harness_runs/20260503_031247/` (`feature_proof=true`, `verdict=artifacts_matched`, green step ledger, portal clear, no movement-error/backtrace markers; same-run lines prove `decision=shadow`, `route=cover_shadow`, `zombie_pressure=2`, then `decision=strike`, `reason=live_vulnerability_window_strike`, `persistent=yes`).
- Night/outside anti-gnome row: `writhing_stalker.live_anti_gnome_bad_loiter_mcw` -> `.userdata/dev-harness/harness_runs/20260503_025712/` (`feature_proof=true`, `verdict=artifacts_matched`, green step ledger, portal clear; same-run line proves `decision=strike`, `reason=live_anti_gnome_night_reachable_probe_strike`, `bad_loiter=2`, `anti_gnome=yes`, `distance=3`).
- Gates: `git diff --check`; `python3 -m py_compile tools/openclaw_harness/startup_harness.py`; JSON validation for changed scenarios/fixture; `make -j4 tests LINTJSON=0 ASTYLE=0 && ./tests/cata_test "[writhing_stalker]" --reporter compact` -> 23 cases / 264 assertions; `./tests/cata_test "[zombie_rider],[flesh_raptor]" --reporter compact` -> 24 cases / 268 assertions.
- Door opening did not land and remains optional/narrow only if later needed.

Required next work:
- Frau review of the live/staged proof packet and closure readiness. Do not rerun solved rows by ritual unless Frau flags a concrete evidence defect.

Non-goals: no burglar stalker, no locked-door solving, no omniscience, no closed-lane reopen by drift, no save-pack/card reruns by ritual.
