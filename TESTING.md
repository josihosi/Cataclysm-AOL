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

### Active validation target - CAOL-WRITHING-STALKER-v0

Current boundary: **monster/stat/spawn footing and deterministic behavior substrate first**. Receipt: `doc/work-ledger.md`. Raw intake: `doc/writhing-stalker-raw-intake-2026-04-30.md`; imagination source: `doc/writhing-stalker-imagination-source-of-truth-2026-04-30.md`; contract: `doc/writhing-stalker-behavior-packet-v0-2026-04-30.md`; testing/playtest ladder: `doc/writhing-stalker-playtest-ladder-v0-2026-04-30.md`.

Greenlit proof ladder:

1. Creature truth: monster JSON/stat/spawn data validates and matches the intended rare singleton first-generation zombie-adjacent predator.
2. Rarity truth: spawn footing does not create ordinary stalker soup; debug/harness can intentionally create one.
3. Interest truth: deterministic helper/evaluator proves human/player evidence and exposed night light outrank empty terrain, smoke stays weak/indirect, cover/edge terrain matters, and zombie pressure increases opportunity rather than threat.
4. Latch truth: recent believable evidence can create a timed/leashed latch, stale evidence decays, and one clue does not become permanent omniscience.
5. Approach truth: stalker prefers cover/darkness/clutter where available and does not direct-beeline across open exposure unless forced by the actual map.
6. Opportunity truth: strike pressure rises when the player is hurt, bleeding, low stamina, noisy, sleeping/resting/crafting, boxed by zombies, or moving through clutter; alert/bright/open player states hold or repel pressure.
7. Strike truth: combat creates short cut/bleed pressure and then either withdraws or changes state; it does not become a tank duel.
8. Withdrawal/cooldown truth: hurt/exposed/focused stalker backs off, cooldown blocks immediate repeat spam, and re-engagement needs refreshed evidence.
9. Persistence truth: any new latch/cooldown state survives save/load, or the packet explicitly avoids new persisted state.
10. Live truth: live/harness packets prove a real shadow/strike scene plus no-omniscient-beeline negative control and exposed/focus retreat or an explicit future-only classification.

Initial proposed live scenarios:

- `writhing_stalker.live_shadow_strike_mcw` — one believable stalk/hold/strike/withdraw scene.
- `writhing_stalker.live_no_omniscient_beeline_mcw` — no valid evidence, no instant beeline/attack.
- `writhing_stalker.live_exposed_retreat_mcw` — latch exists, exposure/focus/hurt state causes hold or withdrawal.

Do not close from JSON alone. Do not close from deterministic tests pretending to be gameplay. Do not close from a debug spawn directly on top of the player and call it stalking. This is horror, not a spreadsheet mugging.

### Closed validation receipt - CAOL-BANDIT-STRUCT-BOUNTY-v0

`CAOL-BANDIT-STRUCT-BOUNTY-v0` is closed/checkpointed green for v0. Closure readout: `doc/bandit-structural-bounty-phase-7-closure-readout-2026-04-30.md`. Canonical contract: `doc/bandit-structural-bounty-overmap-completion-packet-v0-2026-04-30.md`; testing ladder: `doc/bandit-structural-bounty-overmap-testing-ladder-v0-2026-04-30.md`; receipt: `doc/work-ledger.md`.

Credited live run: `.userdata/dev-harness/harness_runs/20260430_115157/`. Credited live claims: idle camp structural forest bounty dispatch without player smoke/light/direct-range bait; same-run candidate economics (`bounty=8`, `known_threat=0`, `confidence=3`, `effective_interest=11`, `decision=scavenge`); stalking-distance keep-open; arrival/harvest; `arrivals=1`; `members_returned=1`; saved harvested/no-repeat state. Non-credit run `.userdata/dev-harness/harness_runs/20260430_114106/` remains red/inconclusive and must not be reused as closure evidence.

Future structural-bounty playtests are greenlit as optional follow-ups, not blockers: live dangerous/turnback Branch A, live reload-resume, two/four-camp wait stress, mixed signal coexistence, and less-blessed natural-ish idle samples.

Recent completed validation targets have been moved to compact receipts in `doc/work-ledger.md` plus their canonical aux/proof docs. Do not re-expand them here unless one becomes the active validation target again.

Current important receipts:
- `CAOL-SZM-LIVE-LABEL-v0` — Smart Zone live coordinate-label proof.
- `CAOL-REAL-FIRE-SIGNAL-v0` — real player fire -> bandit signal response.
- `CAOL-ROOF-HORDE-SPLIT-v0` — split-run roof-fire horde response.
- `CAOL-CANNIBAL-NIGHT-RAID-v0` and `CAOL-CANNIBAL-CONFIDENCE-PUSH-v0` — cannibal behavior closure/confidence uplift.
- `CAOL-BANDIT-CAMP-MAP-RISK-REWARD-v0` — scoped bandit camp-map risk/reward checkpoint.
- `CAOL-FUEL-RED-MULTIDROP-v0` and `CAOL-BANDIT-LIVE-SIGNAL-SITE-BOOTSTRAP-v0` — non-credit/superseded surfaces that must not be reopened by drift.

## Pending probes

Current writhing-stalker next step is not a live harness scene yet. First build the monster/stat/spawn footing and deterministic decision substrate, then promote live probes once the code can honestly explain latch, opportunity, strike, and withdrawal.

Initial greenlit live probes, once implementation footing exists:
- `writhing_stalker.live_shadow_strike_mcw`
- `writhing_stalker.live_no_omniscient_beeline_mcw`
- `writhing_stalker.live_exposed_retreat_mcw`

Preserve `.userdata/smart-zone-audit-live-20260429e/harness_runs/20260429_225644/` as the green Smart Zone Manager live coordinate-label proof. Preserve `.userdata/dev-harness/harness_runs/20260429_180239/` as the green split-run roof-fire horde detection proof and `.userdata/dev-harness/harness_runs/20260429_172847/` as its source player-created roof-fire writeback proof. Preserve `.userdata/dev-harness/harness_runs/20260429_162100/` as green player-lit source-zone fire -> bandit signal proof. Preserve `.userdata/dev-harness/harness_runs/20260430_115157/` as green structural-bounty v0 proof and `.userdata/dev-harness/harness_runs/20260430_114106/` as red/non-credit structural-bounty postmortem. Do not rerun solved rows as ritual.

Future-only watchlist unless Schani/Josef explicitly promotes it:

- Natural three/four-site player-pressure behavior and true zero-site idle baseline remain decision/watchlist items, not current requirements.
- A stricter roof-horde positive `tracking_intensity` proof remains optional/future-only; current green roof proof is retarget/movement-budget metadata after live roof-fire horde signaling.
- Full Smart Zone process-reload disk persistence can be promoted later if Josef wants that stricter audit; the current green proof covers live create/inspect/close-save/reopen coordinate labels.

Stale runs `.userdata/dev-harness/harness_runs/20260429_093118/`, `.userdata/dev-harness/harness_runs/20260429_093509/`, `.userdata/dev-harness/harness_runs/20260429_095021/`, `.userdata/dev-harness/harness_runs/20260429_122807/`, and `.userdata/dev-harness/harness_runs/20260429_122955/` are retained as postmortem evidence only, not fire proof surfaces.

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
