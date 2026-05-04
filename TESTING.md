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

### Promotion / closure hygiene

Before promoting, closing, or handing off a lane, confirm that `TESTING.md` pending probes still match the active `Plan.md` lane. If the pending-probe text points at an older slice, fix it before Andi uses it as execution truth.

### Bandit overmap-proof rule

For the remaining bandit AI proof packets, single-turn deterministic checks are **not** enough by themselves.
The honest bar now includes real overmap-side multi-turn scenario proof, up to `500` turns where needed, with explicit per-scenario goals and tuning metrics.

## Current validation targets

### No active validation target

`CAOL-JOSEF-LIVE-DEBUG-BATCH-v0` is checkpointed green agent-side and awaiting Schani/Frau boundary review. Its slice receipts are:

- shakedown UI/payment: `doc/shakedown-pay-fight-npc-trade-ui-proof-v0-2026-05-03.md`
- defended-camp current-pass standoff/hot-loot: `doc/defended-camp-scout-standoff-hot-loot-proof-v0-2026-05-03.md`
- multi-z/roof-z dispatch fallback: `doc/multi-z-roof-dispatch-fallback-proof-v0-2026-05-03.md`
- hostile-camp escalation: `doc/hostile-camp-toll-escalation-proof-v0-2026-05-03.md`
- all-light-source adapter: `doc/all-light-source-live-adapter-proof-v0-2026-05-04.md`
- patrol aggression/alarm/report hygiene: `doc/camp-patrol-aggression-alarm-report-hygiene-proof-v0-2026-05-04.md`
- writhing stalker distance/sight/threat-drop: `doc/writhing-stalker-distance-sight-threat-drop-live-proof-v0-2026-05-04.md`
- NPC sorting debounce: `doc/npc-sorting-failure-debounce-proof-v0-2026-05-04.md`
- debug spawn options: `doc/debug-spawn-overmap-threat-options-proof-v0-2026-05-04.md`
- locker/basecamp equipment consistency: `doc/locker-basecamp-equipment-consistency-proof-v0-2026-05-04.md`
- Monsterbone spear: `doc/monsterbone-spear-proof-v0-2026-05-04.md`

Deferred hardening/watchlist rows such as deeper defended-camp sight/smoke playfeel, full live cannibal raid/contact, local active-zombie devirtualization, positive `tracking_intensity`, and Monsterbone live camp feel are future-only unless Schani/Josef promotes a specific target. Do not rerun closed rows as ritual.

### Closed validation receipt - CAOL-WRITHING-STALKER-THREAT-DISTRACTION-HANDOFF-v0

`CAOL-WRITHING-STALKER-THREAT-DISTRACTION-HANDOFF-v0` is closed/checkpointed green v0 after Frau review. Contract: `doc/writhing-stalker-threat-distraction-handoff-packet-v0-2026-05-02.md`; imagination source: `doc/writhing-stalker-threat-distraction-handoff-imagination-source-2026-05-02.md`; handoff: `doc/writhing-stalker-threat-distraction-handoff-handoff-v0-2026-05-02.md`; proof: `doc/writhing-stalker-threat-distraction-live-staged-proof-v0-2026-05-03.md`.

Credited current-build staged/live rows: high-threat/allies retreat/stalk `.userdata/dev-harness/harness_runs/20260503_021310/`; zombie/distraction clean shadow-then-strike `.userdata/dev-harness/harness_runs/20260503_031247/`; night/outside anti-gnome strike `.userdata/dev-harness/harness_runs/20260503_025712/`. Gate at closure: `git diff --check`; `python3 -m py_compile tools/openclaw_harness/startup_harness.py`; scenario/fixture JSON validation; `make -j4 tests LINTJSON=0 ASTYLE=0 && ./tests/cata_test "[writhing_stalker]" --reporter compact` -> 23 cases / 264 assertions; spillover guard `./tests/cata_test "[zombie_rider],[flesh_raptor]" --reporter compact` -> 24 cases / 268 assertions.

Claim boundary: staged/live feature-path proof plus deterministic seam coverage only. No natural random discovery, full natural retreat pathing, broad house navigation, door opening, burglar/locked-door solving, or general ecosystem claim.

### Handoff-boundary validation receipt - CAOL-JOSEF-PLAYTEST-SAVE-PACK-v0

`CAOL-JOSEF-PLAYTEST-SAVE-PACK-v0` is at a handoff boundary, not an active Andi lane. Contract: `doc/caol-josef-playtest-save-pack-packet-v0-2026-05-02.md`; imagination source: `doc/caol-josef-playtest-save-pack-imagination-source-2026-05-02.md`; handoff: `doc/caol-josef-playtest-save-pack-handoff-v0-2026-05-02.md`; working card: `doc/caol-josef-playtest-save-pack-card-v0-2026-05-02.md`.

Card checkpoint: six ready staged rows are listed for Josef (camp locker weather/service, bandit first-demand contact, cannibal night pressure, flesh raptor crowded-arc skirmisher, zombie rider cover/wounded contrast, writhing stalker hit-fade/light/zombie-side pressure). Optional staged bandit contrast footing is ready and caveated in the card/doc ledger. All ready rows record current-build load/start-state footing and portal-storm status; no current validation target calls for rerunning them.

### Archived closed receipts

Detailed closed validation history has been trimmed out of this active testing file. For older closed lanes and non-credit proof, use `SUCCESS.md`, `Plan.md`, `doc/work-ledger.md`, linked `doc/*proof*` / packet files, and git history. This includes closed bandit signal adapter, portal-storm warning-light, zombie rider close-pressure, camp locker zone playtests, flesh raptor, stalker pattern/live/zombie-shadow/hit-fade, multi-camp, roof-horde, Smart Zone, fire, bandit, cannibal, and older basecamp/locker packets.

## Pending probes

No active Andi-side probe is queued in current repo canon. `CAOL-JOSEF-LIVE-DEBUG-BATCH-v0` is at a review boundary after Slice 11; further proof requires Schani/Josef promotion of a named deferred row or a new greenlit packet.

Closed zombie-rider, flesh-raptor, writhing-stalker, roof-horde, Smart Zone, fire, bandit, and multi-camp proof trains are represented by `SUCCESS.md`, `Plan.md`, `doc/work-ledger.md`, linked aux proof docs, and git history. Do not rerun solved rows as ritual.

Future-only watchlist unless Schani/Josef explicitly promotes it:
- Natural three/four-site player-pressure behavior and true zero-site idle baseline remain decision/watchlist items, not current requirements.
- A stricter roof-horde positive `tracking_intensity` proof remains optional/future-only; current green roof proof is retarget/movement-budget metadata after live roof-fire horde signaling.
- Full Smart Zone process-reload disk persistence can be promoted later if Josef wants that stricter audit; the current green proof covers live create/inspect/close-save/reopen coordinate labels.

If a later live probe is promoted:
- build the current runtime first when binary freshness matters;
- use one named scenario/command path;
- extract only decisive report/log fields into context;
- after two same-blocker attempts, consult Frau Knackal before attempt 3; after four unresolved attempts, package implemented-but-unproven state for Josef and move to the next greenlit unblocked target.

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
