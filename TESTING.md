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
If another good agent-side lane exists, keep moving there.
If several human-only judgments are likely soon, batch them instead of sending tiny separate asks.

### Anti-churn rule

Do not keep rerunning the same startup or test packet when it is no longer the missing evidence class.
If startup/load is already green, and the missing proof is live behavior, then the next probe must target live behavior.
If a target is merely waiting on Josef, do not keep revalidating it unless the code changed.

---

## Current relevant evidence

### Basecamp bark / board-routing checkpoint
Latest relevant agent-side packet:
- `make -j4 tests`
- `./tests/cata_test "[camp][basecamp_ai]"`
  - passed at `289 assertions in 1 test case`
  - includes the real `handle_heard_camp_request` router path, not only helper builders
  - proves structured `show_board` emits the richer handoff snapshot with `planner_move` + overmap context when a camp origin exists
  - proves spoken `show me the board` stays on the concise human-facing bark path instead of dumping the full snapshot

Meaning:
- the board-routing proof slice is closed for now
- this packet should only be rerun if the relevant camp routing code changes again
- this area has no standing pending handoff packet anymore

### Locker Zone v1 baseline
Latest relevant agent-side baseline for the checkpointed locker tree:
- `make -j4 tests`
- `./tests/cata_test "[camp][locker]"`
  - passed at `94 assertions in 8 test cases` on dirty `1a72369cfb`
  - rerun passed again after the live probe at `94 assertions in 8 test cases`
  - deterministic logs show human-readable locker `before` / `plan` / `after` summaries
- `make -j4 TILES=1 cataclysm-tiles`
- `python3 tools/openclaw_harness/startup_harness.py start --profile dev --world 'Sandy Creek'`
  - passed cleanly on `.userdata/dev/harness_runs/20260405_110124`
- live current-binary probe on `dev` / `Sandy Creek`
  - restored the missing `CAMP_LOCKER` character-zone fixture from `Sandy Creek.bak.20260405_111739`
  - launched `./cataclysm-tiles --userdir .userdata/dev/ --world 'Sandy Creek'`
  - opened the in-game wait menu and chose wait-till-midnight (`m`)
  - current `debug.log` then emitted a fresh locker packet for **Bruna Priest** on `1a72369cfb-dirty`:
    - `camp locker: before Bruna Priest ... locker=[none]`
    - `camp locker: plan for Bruna Priest ... changes=[shirt dedupe keep=polo shirt<polo_shirt> drop=[flotation vest<flotation_vest>]; bag dedupe keep=messenger bag<mbag> drop=[leather belt<leather_belt>]]`
    - `camp locker: after Bruna Priest ... locker=[shirt=[flotation vest<flotation_vest>]; bag=[leather belt<leather_belt>]]`
  - screenshots/live capture artifacts: `.userdata/dev/live_probe/`

Meaning:
- locker groundwork, planner/service logic, and queue/reservation tail are covered agent-side
- the current binary compiles, relinks, starts cleanly, and has one real downtime-driven live locker packet on the current binary
- this lane is no longer missing technical proof; only revisit it if later code changes break the locker path again

---

## Pending probes

None for the finished Basecamp bark / craft slice.
If this area is reopened later, add only the new missing evidence instead of resurrecting the old handoff packet by habit.

---

## Reusable commands

Use these when they are actually the missing evidence, not as ritual.

### Narrow deterministic locker check
- `./tests/cata_test "[camp][locker]"`

### Narrow deterministic Basecamp bark / routing check
- `./tests/cata_test "[camp][basecamp_ai]"`

### Fresh full test rebuild on this Mac
- `make -j4 tests`

### Fresh tiles relink on this Mac
- `make -j4 TILES=1 cataclysm-tiles`

### Startup/load smoke
- `python3 tools/openclaw_harness/startup_harness.py start --profile dev --world 'Sandy Creek'`

## Local build caveat

On this Mac, treat top-level `make -j4 tests` as the reliable path for a fresh `cata_test`.
Avoid treating `make -C tests cata_test` as authoritative here; it has been a repeated source of toolchain/stale-build nonsense.
