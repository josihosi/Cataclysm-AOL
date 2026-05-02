# Andi handoff

Recent state boundary: `CAOL-WRITHING-STALKER-HIT-FADE-RETREAT-DISTANCE-v0` is closed/checkpointed green v0.

Proof/readout: `doc/writhing-stalker-hit-fade-retreat-distance-proof-v0-2026-05-02.md`.

Evidence summary:
- original flesh-raptor reference checked: generic `HIT_AND_RUN` adds `effect_run` after melee; stalker intentionally dropped it so the custom planner can own bounded burst/fade cadence;
- deterministic gate green: `git diff --check && make -j8 tests LINTJSON=0 ASTYLE=0 && ./tests/cata_test "[writhing_stalker]"` -> `All tests passed (206 assertions in 17 test cases)`;
- current-build gate green: `./just_build_macos.sh > /tmp/caol-writhing-stalker-hit-fade-tiles-build-current.log 2>&1` exited `0` and linked `cataclysm-tiles`;
- fresh staged-live feature proof: `writhing_stalker.live_hit_fade_retreat_mcw` -> `.userdata/dev-harness/harness_runs/20260502_113738/`, `feature_proof=true`, `verdict=artifacts_matched`, `step_ledger_status=green_step_local_proof`, no runtime warnings, with `decision=strike`, `decision=withdraw`, `decision=cooling_off`, `burst=0/2`, `burst=1/2`, `burst=2/2`, `retreat_distance=8`, `cooldown=yes`.

In-game expectation: the stalker should no longer do generic one-hit `HIT_AND_RUN`; favorable vulnerability can produce a short 2-4 hit burst, while light/focus/allied support/injury shorten persistence and increase retreat distance/caution. After the burst, live planning asks for about 8+ tiles of breathing room where pathing allows.

Caveats: staged-but-live McWilliams proof, not natural random discovery; prior watched seed `.userdata/dev-harness/harness_runs/20260502_015032/` remains yellow/debug footing only due to runtime-version mismatch; final human taste is optional/future-only.

Recommended next action: Schani/plans-aux should confirm or promote the next active item from `Plan.md` greenlit backlog. Do not reopen closed portal-storm, bandit, visions, camp-locker, or writhing-stalker lanes by ritual.
