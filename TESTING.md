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

### Active validation target - Smart Zone Manager harness-audit retry packet v0

`Smart Zone Manager harness-audit retry packet v0` is now active because Josef identified a direct live UI observable: the Zone Manager shows relative coordinates next to each zone (`2E`, etc.). Contract: `doc/smart-zone-manager-harness-audit-retry-packet-v0-2026-04-28.md`; imagination source: `doc/smart-zone-manager-harness-audit-retry-imagination-source-of-truth-2026-04-28.md`.

Evidence burden: run the real Zone Manager generation path on a current-runtime loadable fixture, then inspect/capture the generated zone list coordinates. Same-coordinate labels are direct evidence that zones collapsed onto one tile; distinct expected relative offsets are direct evidence for separated placement. Pair the coordinate read with saved zone metadata and save/reopen state where possible. The old `20260428_151053` run remains non-green and cannot be laundered into closure.

Validation policy for this target: `git diff --check`; narrow compile/test only if harness/runtime/Smart Zone code changes; relevant deterministic `clzones` tests only as support if Smart Zone code changes; one named harness probe/handoff with step-local ledger and coordinate-label evidence. If the UI coordinate read is blocked, record the exact missing primitive or fixture problem instead of substituting load-only proof.

### Recently completed validation target - Bandit camp-map risk/reward dispatch planning packet v0

`Bandit camp-map risk/reward dispatch planning packet v0` is closed as **scoped live/product checkpoint green**. Frau product review accepted the current matrix as sufficient for this checkpoint and did not require a second-fixture bias variant before closure.

Checkpoint evidence boundary:

- deterministic/code support covers the camp-owned `camp_intelligence_map`, serialization, active target OMT persistence, scout-return writeback, live signal camp-map lead writes, remembered-lead selection through live dispatch cadence/reporting, two-OMT ordinary scout stand-off, 720-minute scout return clock, 2/4/5/7/10 roster/reserve sizing, active-outside dogpile blocking, wounded/unready and killed-member shrinkage, bounded stockpile pressure, high-threat non-escalation, prior defender-loss pressure, prior bandit-loss cooling, larger-than-scout stalk sizing, and no-opening hold/return;
- feature-path proof covers sight-avoid (`20260428_173626`), vanished-signal redispatch (`20260428_185947`), 2/5/10 variable-roster sizing (`20260428_202044`, `20260428_192059`, `20260428_193621`), no-opening stalk-pressure (`20260428_195617`), high-threat/low-reward hold (`20260428_200600`), active-outside dogpile-block (`20260428_200434`), shakedown/toll-control (`20260428_204454`, `20260428_204630`, `20260428_204813`), same-fixture high-threat repeatability (`20260428_211105`, `20260428_211153`), and optional empty-camp retirement sanity (`20260428_214542`).

Recorded gates: `python3 -m py_compile tools/openclaw_harness/startup_harness.py`; relevant scenario/fixture `python3 -m json.tool` checks; `python3 tools/openclaw_harness/proof_classification_unit_test.py`; `git diff --check`; `make -j4 TILES=1 cataclysm-tiles LINTJSON=0 ASTYLE=0`; `./tests/cata_test "[bandit_live_world]"`; `./tests/cata_test "[bandit][live_world][camp_map]" --success`; and the named harness probes. `make astyle-diff` remains locally blocked by missing `astyle`.

Caveats retained: player-created fire/smoke/light scenario 1 remains blocked behind the fuel/writeback gate and is not credited; same-fixture repeatability is not second-fixture anti-bias proof; the no-opening proof is not opening-present escalation proof.

### Recently completed validation target - Cannibal camp night-raid behavior packet v0

The required cannibal live matrix scenarios are checkpointed green: day smoke/stalk pressure, night local-contact pack attack, exposed/recent-sight hold-off, daylight/high-threat negative, saved-state persistence, and day-smoke repeatability. The optional labeled bandit-control contrast is non-blocking unless product review explicitly reopens it.

### Recently completed validation target - C-AOL debug-proof finish stack

`C-AOL debug-proof finish stack v0` is complete and ready for Schani review. Retain the old implemented-but-unproven boundaries for real player-lit fire and Smart Zone Manager live layout unless current canon explicitly promotes their retry packets.

---

## Pending probes

Active validation target: **Smart Zone Manager harness-audit retry packet v0**. Use the Zone Manager's visible relative coordinate labels as the primary live proof for separated-vs-lumped generated zones. Do not substitute deterministic geometry, startup/load, or old non-green Smart Zone runs for this coordinate-read proof.

Queued after this Smart Zone checkpoint, per `Plan.md` / greenlit stack:

1. `Generic clean-code boundary review packet v0`: report-only boundary review, now queued after the active Smart Zone retry.
2. `Cannibal camp confidence-push live playtest packet v0`: confidence uplift for already-closed cannibal behavior, not a redesign or failure reopen.
3. `Player-lit fire and bandit signal verification packet v0`: still blocked behind the fuel/writeback gate.
4. `Roof-fire horde detection proof packet v0`: still blocked behind real player-lit fire.
5. `C-AOL live AI performance audit packet v0`: bottom-of-stack measured performance audit.

If a later live probe is needed:
- build the current runtime first when binary freshness matters;
- use one named scenario/command path;
- extract only decisive report/log fields into context;
- after two same-blocker attempts, consult Frau Knackal before attempt 3; after four unresolved attempts, package implemented-but-unproven state for Josef and move on.

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
