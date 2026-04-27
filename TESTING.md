# TESTING

Current validation policy and current evidence only.

This file is not a trophy wall.
Remove stale or completed fluff instead of stacking crossed-off test history forever.

## Validation policy

Use the **smallest evidence that honestly matches the change**.

- **Docs-only change** -> no compile
- **Small local code change** -> narrow build and the narrowest relevant test target
- **Broad or risky code change** -> broader rebuild and relevant filtered tests
- **Before a Josef handoff** or after suspicious stale-binary behavior -> rebuild the real targets and rerun the real smoke path

### Agent vs Josef

Schani should do agent-side playtesting first whenever the harness or direct in-game probing can answer the question.
Josef should be asked only for:
- product judgment
- tone/feel
- human-priority choices
- genuinely human-only interaction

Josef being unavailable is **not** a blocker by itself.
Josef self-testing is **not** a plan blocker and does not belong in the active success state as a gate.
If Josef-specific checks are useful, write them down as non-blocking notes so he can run them later from his own list.
If another good agent-side lane exists, keep moving there.
If several human-only judgments are likely soon, batch them instead of sending tiny separate asks.

### Anti-churn rule

Do not keep rerunning the same startup or test packet when it is no longer the missing evidence class.
If startup/load is already green, and the missing proof is live behavior, then the next probe must target live behavior.
If a target is merely waiting on Josef, do not keep revalidating it unless the code changed.

Current debug-stack attempt rule for the same item/blocker:
- attempts 1-2 may be Andi solo retries, including multiple focused harness attempts in one cron run when each attempt changes setup, instrumentation, or evidence class
- before attempt 3, consult Frau Knackal
- attempts 3-4 are the final changed retries after consultation
- after attempt 4, if code is implemented but proof still fails, add a concise implemented-but-unproven packet to Josef's playtest package and move to the next greenlit debug note; do not close it and do not park it as dead

### Test-to-game wiring rule

A test is not allowed to impersonate implementation. Before claiming gameplay behavior, identify the live code path that consumes the tested seam and name the evidence class that proves it: unit/evaluator, playback/proof packet, live source hook, harness/startup, screen, save inspection, or artifact/log. Deterministic-only packets may close only as deterministic-only packets; if the contract says the game does something, the proof must reach the game path or the claim stays open.

### Bandit overmap-proof rule

For the remaining bandit AI proof packets, single-turn deterministic checks are **not** enough by themselves.
The honest bar now includes real overmap-side multi-turn scenario proof, up to `500` turns where needed, with explicit per-scenario goals and tuning metrics.

## Current validation targets

### Active validation target - C-AOL debug-proof finish stack

`C-AOL debug-proof finish stack v0` is active for another honest proof pass.

Retained evidence classification for `Bandit live signal + site bootstrap correction v0`:
- raw saved `fd_fire` / `fd_smoke` fixtures prove map-field reader / consumer behavior only
- synthetic smoke proof proves only synthetic smoke-source/live-signal behavior and must be labeled as such
- player-fire product proof still requires the real brazier/wood/lighter/player-action chain and fresh matched-site evidence
- threshold-surviving light proof for the synthetic loaded-map `fd_fire` source path is now covered by run `.userdata/dev-harness/harness_runs/20260427_114034/`: current-runtime probe at commit `daa2f1694c`, night `light_packets=1`, horde light signal, and `matched_light_sites=1` / `refreshed_sites=1`; this is not full player-lit-fire proof
- remaining player-fire product proof still requires the real brazier/wood/lighter/player-action chain plus paired no-signal/control evidence; post-Frau attempt 3 (`bandit.live_world_nearby_camp_real_fire_exact_items_tile_audit_mcw`, run `.userdata/dev-harness/harness_runs/20260427_124445/`) used exact-id fixture items (`brazier`, `2x4`, charged `lighter`) and stopped at the required midpoint because `tile_fire_audit.east.json` found no target-tile `fd_fire`, with saved-player inspection showing the GUI route had not actually moved/activated those items; deterministic non-GUI primitive `fire_start_activity_exact_brazier_wood_lighter_creates_fd_fire` now proves the current-build fire-start activity creates target-tile `fd_fire` from exact `f_brazier` + `2x4` + charged `lighter`, but this is activity-seam proof rather than full live GUI recipe proof; if the remaining live bridge is moved to Josef's playtest package, keep fire creation proof separate from bandit scan consumption from that player fire unproven

### Recently closed validation references

Use the auxiliary docs / `SUCCESS.md` for details. Current short closure map:
- `Basecamp job spam debounce + locker/patrol exceptions packet v0`: stable-cause debounce for repeated completion/missing-tool/no-progress camp chatter; typed `[camp][locker]` / `[camp][patrol]` reports preserve first and changed states while compressing repeats; deterministic message tests, focused `[camp][patrol]` / `[camp][locker]`, and touched-object compile passed.
- `Bandit live-wiring audit + visible-light horde bridge correction v0`: loaded-map visible fire/light -> horde signal bridge proof; not player-lit fire proof.
- `Bandit local sight-avoid + scout return cadence packet v0`: deterministic proof plus live/harness sight-avoid, return-home, and writeback proof; no later redispatch tuning claimed.
- `Smart Zone Manager v1 Josef playtest corrections`: deterministic zone-id/type/option and save/reload proof.
- `Basecamp medical consumable readiness v0`: deterministic camp/locker proof for bounded bandage-family readiness and cap behavior.
- `Basecamp locker armor ranking + blocker removal packet v0`: deterministic camp/locker proof for generic full-body/protective ranking, blocker clearing, damaged-candidate rejection without repeated requeue/equip churn, ballistic armor preservation, and `[camp][locker]` regression coverage.

---

## Pending probes

No active live GUI probe is required for the closed job-spam debounce lane. For the active debug-proof finish stack, use live/harness/product-path proof where the contract asks for product behavior; deterministic-only evidence is not enough for the remaining bandit live-signal claims.

If a later live probe is needed:
- build the current runtime first when binary freshness matters
- use one named scenario/command path
- extract only decisive report/log fields into context
- multiple focused attempts in one run are allowed when each changes setup/instrumentation/evidence and output stays small
- after two same-item failures, consult Frau Knackal before attempt 3; after four total failures, write the implemented-but-unproven item to Josef's playtest package instead of looping

---

## Reusable commands

Use these when they are actually the missing evidence, not as ritual.

### Narrow camp deterministic pass after a code slice lands
- `git diff --check`
- `make -j4 obj/basecamp.o tests/faction_camp_test.o tests LINTJSON=0 ASTYLE=0`
- focused `./tests/cata_test` filters for the touched camp/locker/patrol reporting path

### Plan status summary command
- `python3 tools/plan_status_summary.py --self-test`
- `python3 tools/plan_status_summary.py /plan`
- `python3 tools/plan_status_summary.py /plan active`
- `python3 tools/plan_status_summary.py /plan greenlit`
- `python3 tools/plan_status_summary.py /plan parked`

### Fresh tiles rebuild only if a later handoff really needs live proof
- `make -j4 TILES=1 cataclysm-tiles`

## Local build caveat

On this Mac, treat top-level `make -j4 tests` as the reliable path for a fresh `cata_test`.
Avoid treating `make -C tests cata_test` as authoritative here; it has been a repeated source of toolchain/stale-build nonsense.
Also: if you actually need a fresh tiles binary, use `make -j4 TILES=1 cataclysm-tiles`; plain `make cataclysm-tiles` is not an honest rebuild path here.
