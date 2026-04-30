# Andi handoff: CAOL-WRITHING-STALKER-v0

## Current canon state

`CAOL-WRITHING-STALKER-v0` is the **ACTIVE / GREENLIT IMPLEMENTATION PACKET**.

Authoritative canon is `Plan.md`, with `TODO.md`, `SUCCESS.md`, `TESTING.md`, and `doc/work-ledger.md` aligned downstream. This handoff is only a terse executor packet; if it ever disagrees with those files, repair this file from canon instead of treating it as truth.

The previous Bandit structural bounty handoff is superseded. `CAOL-BANDIT-STRUCT-BOUNTY-v0` is closed/checkpointed green v0 in canon and must not pull execution away from the current creature packet.

## Current state boundary

Monster/stat/spawn footing, deterministic AI-decision substrate, the live monster-plan seam, and the three basic live writhing-stalker behavior proofs are implemented and validated.

Green writhing-stalker boundary now includes:

- `mon_writhing_stalker` JSON/stat footing and rare singleton `GROUP_ZOMBIE` spawn entry.
- Deterministic interest/latch/approach/opportunity/withdraw/cooldown tests in `src/writhing_stalker_ai.*` and `tests/writhing_stalker_test.cpp`.
- Live `monster::plan()` seam routing `mon_writhing_stalker` through `writhing_stalker::evaluate_live_response()` for local-evidence gating, shadow destinations, strike targeting, withdrawal, and cooldown behavior.
- Support live spawn/save footing: `.userdata/dev-harness/harness_runs/20260430_161342/`.
- Basic target/plan-turn live seam proof: `.userdata/dev-harness/harness_runs/20260430_161535/`.
- Exposed/focused withdrawal proof: `.userdata/dev-harness/harness_runs/20260430_163626/`.
- Midnight vulnerable-player shadow/strike/cooldown proof: `.userdata/dev-harness/harness_runs/20260430_170528/`.
- No-omniscience negative-control proof: `.userdata/dev-harness/harness_runs/20260430_173555/`.

V0 adds no new persisted latch state; cooldown uses existing monster effect state.

## Next executor slice

Run the mixed-hostile metrics packet:

- scenario target: `performance.mixed_hostile_stalker_horde_mcw`
- contract: `doc/mixed-hostile-stalker-horde-performance-playtest-v0-2026-04-30.md`

The packet must report metrics with:

- one bandit camp / hostile live-world site;
- one cannibal camp / hostile live-world site;
- exactly one `mon_writhing_stalker` at plausible distance;
- one overmap horde or horde-pressure fixture;
- runtime commit/dirty state;
- setup proof all four requested actors exist;
- in-game window and sampled turns/waited minutes;
- harness wall-clock;
- per-turn min/median/p95/max where available;
- `bandit_live_world perf` min/median/max plus slice maxima;
- stalker and horde timing, or explicit `not instrumented` where unavailable;
- debug/log spam, stability, and playability/tuning note.

Do **not** drift back into wiring/promoting the three basic live writhing-stalker scenarios. They are green. The next honest missing evidence is the mixed-hostile performance/tuning readout.

## Credited evidence for current boundary

- JSON/schema and rarity gates:
  - `python3 -m json.tool data/json/monsters/zed_misc.json`
  - `python3 -m json.tool data/json/monstergroups/zombies.json`
  - focused Python JSON audit for singleton `GROUP_ZOMBIE` entry.
- Deterministic/code gates:
  - `make -j4 obj/writhing_stalker_ai.o obj/monmove.o tests/writhing_stalker_test.o tests LINTJSON=0 ASTYLE=0`
  - `./tests/cata_test "[writhing_stalker]"`
- Live behavior gates:
  - `writhing_stalker.live_spawn_footing_mcw` -> `.userdata/dev-harness/harness_runs/20260430_161342/`
  - `writhing_stalker.live_plan_seam_mcw` -> `.userdata/dev-harness/harness_runs/20260430_161535/`
  - `writhing_stalker.live_exposed_retreat_mcw` -> `.userdata/dev-harness/harness_runs/20260430_163626/`
  - `writhing_stalker.live_shadow_strike_mcw` -> `.userdata/dev-harness/harness_runs/20260430_170528/`
  - `writhing_stalker.live_no_omniscient_beeline_mcw` -> `.userdata/dev-harness/harness_runs/20260430_173555/`
- Latest no-omniscience proof classification:
  - `feature_proof=true`
  - `evidence_class=feature-path`
  - 14/14 green steps
  - same-run `target_probe ... target=no ... sees_player=no`
  - no `target=yes`, no `sees_player=yes`, no `writhing_stalker live_plan:`, no strike/shadow/cooldown in the no-evidence window.

## Non-goals/cautions

- Do **not** reopen Bandit structural bounty, Smart Zone, fire/signal, roof-horde, release, or cannibal lanes from this handoff.
- Do **not** implement bandit shakedown/camp logic, Basecamp economy hooks, common spawn spam, teleport ambushes, global exact scans, or long-term nemesis arcs in writhing stalker v0.
- Do **not** close performance from startup/load, setup-only, or deterministic evidence. The mixed-hostile packet needs live-path metrics or an honest missing-metric classification.
- Do **not** treat Josef availability as a blocker. If the metrics packet can be agent-run, run it; reserve Josef for product feel/tuning judgment.

## Canonical anchors

- Active roadmap: `Plan.md`
- Active queue: `TODO.md`
- Success ledger: `SUCCESS.md`
- Testing ledger: `TESTING.md`
- Receipt ledger: `doc/work-ledger.md` (`CAOL-WRITHING-STALKER-v0`)
- Raw intake: `doc/writhing-stalker-raw-intake-2026-04-30.md`
- Imagination source: `doc/writhing-stalker-imagination-source-of-truth-2026-04-30.md`
- Contract: `doc/writhing-stalker-behavior-packet-v0-2026-04-30.md`
- Testing/playtest ladder: `doc/writhing-stalker-playtest-ladder-v0-2026-04-30.md`
- Mixed-hostile performance packet: `doc/mixed-hostile-stalker-horde-performance-playtest-v0-2026-04-30.md`

## Superseded handoff note

This file used to say the next executor should wire the live seam and promote the three basic writhing-stalker scenarios. That is stale. The live seam plus `writhing_stalker.live_shadow_strike_mcw`, `writhing_stalker.live_no_omniscient_beeline_mcw`, and `writhing_stalker.live_exposed_retreat_mcw` are green; the next executor should advance `performance.mixed_hostile_stalker_horde_mcw`.
