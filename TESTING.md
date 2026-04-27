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

Smart Zone Manager live layout separation correction is now **implemented-but-unproven live**: deterministic geometry/separation assertions and the explicit overlap allowlist are green, but the clean GUI macro still could not honestly inspect the generated player-visible layout. The manual close recipe is packaged as `smart-zone-live-layout-separation-correction` in `/Users/josefhorvath/.openclaw/workspace/runtime/josef-playtest-package.md`; do not rerun the contaminated old McWilliams macro as closure proof.

Current unblocked target: **Bandit local standoff / scout return live correction**. Josef's 2026-04-27 live test grants smoke attraction, but reports the scout/hold-off bandit still gets too close and does not visibly time out/return home. Required evidence:
- recent live log/save inspection answering whether `active_sortie_started_minutes` / `active_sortie_local_contact_minutes` are present and advancing in the current save;
- deterministic coverage for any changed standoff distance/goal logic so hold-off does not collapse into adjacent/neighboring-OMT crowding;
- live or harness proof that is not merely a pre-aged timer fixture unless the report labels it as fixture-only seam proof;
- if the live route cannot be honestly proven inside the attempt budget, a Josef playtest package with the exact manual close recipe.

Retained evidence classification for `Bandit live signal + site bootstrap correction v0`:
- raw saved `fd_fire` / `fd_smoke` fixtures prove map-field reader / consumer behavior only
- synthetic smoke proof proves only synthetic smoke-source/live-signal behavior and must be labeled as such
- if Josef reopens player-fire product proof, it still requires the real brazier/wood/lighter/player-action chain and fresh matched-site evidence; current agent-side status is implemented-but-unproven in the playtest package
- threshold-surviving light proof for the synthetic loaded-map `fd_fire` source path is now covered by run `.userdata/dev-harness/harness_runs/20260427_114034/`: current-runtime probe at commit `daa2f1694c`, night `light_packets=1`, horde light signal, and `matched_light_sites=1` / `refreshed_sites=1`; this is not full player-lit-fire proof
- full player-fire product proof is now in `runtime/josef-playtest-package.md` as `bandit-live-signal-real-fire-product-proof` implemented-but-unproven: post-Frau attempt 3 (`bandit.live_world_nearby_camp_real_fire_exact_items_tile_audit_mcw`, run `.userdata/dev-harness/harness_runs/20260427_124445/`) failed the target-tile `fd_fire` midpoint audit, deterministic non-GUI primitive `fire_start_activity_exact_brazier_wood_lighter_creates_fd_fire` proves only the activity seam, and final activity-bridge experiment `.userdata/dev-harness/harness_runs/20260427_134253/` did not leave audited target-tile `fd_fire`/`fd_smoke` before live bandit product closure

### Recently closed validation references

Use the auxiliary docs / `SUCCESS.md` for details. Current short closure map:
- `Basecamp job spam debounce + locker/patrol exceptions packet v0`: stable-cause debounce for repeated completion/missing-tool/no-progress camp chatter; typed `[camp][locker]` / `[camp][patrol]` reports preserve first and changed states while compressing repeats; deterministic message tests, focused `[camp][patrol]` / `[camp][locker]`, and touched-object compile passed.
- `Bandit live-wiring audit + visible-light horde bridge correction v0`: loaded-map visible fire/light -> horde signal bridge proof; not player-lit fire proof.
- `Bandit local sight-avoid + scout return cadence packet v0`: REOPENED for 2026-04-27 live product evidence that the scout/hold-off bandit still crowds too close and is not timing out/returning home in the current save; earlier pre-aged/fixture proof is seam evidence only.
- `Smart Zone Manager v1 Josef playtest corrections`: implemented-but-unproven live; deterministic geometry/separation proof is green, while clean live/UI layout inspection is in Josef's playtest package.
- `Basecamp medical consumable readiness v0`: deterministic camp/locker proof for bounded bandage-family readiness and cap behavior.
- `Basecamp locker armor ranking + blocker removal packet v0`: deterministic camp/locker proof for generic full-body/protective ranking, blocker clearing, damaged-candidate rejection without repeated requeue/equip churn, ballistic armor preservation, and `[camp][locker]` regression coverage.

---

## Pending probes

No active live GUI probe is required for the closed job-spam debounce lane. For the active debug-proof finish stack, use live/harness/product-path proof where the contract asks for product behavior; deterministic-only evidence is not enough for product claims. The current unblocked target is the reopened bandit local standoff / scout return live correction; do not spin another bandit player-fire loop or another Smart Zone GUI macro unless the harness primitive materially changes.

Queued process validation target: **C-AOL harness trust audit + proof-freeze packet v0**. When activated, the evidence target is the harness itself: every primitive/keystroke/setup step needs a precondition, action, expected state, screenshot or exact metadata proof, failure rule, artifact path, and pass/yellow/red/blocked verdict. Use one canonical disposable save/profile where possible; any extra fixture must be justified with provenance. A load-and-close run remains startup/load proof only. Failed spawns, wrong screens, missing target fields, stale binaries/profiles, and missing save metadata must produce explicit non-green results rather than feature-proof claims.

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
