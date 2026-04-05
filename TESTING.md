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

### Locker Zone v1 checkpoint
Latest relevant agent-side locker packet:
- `make -j4 tests`
- `./tests/cata_test "[camp][locker]"`
  - passed at `121 assertions in 10 test cases` on dirty `ab9cd121f8`
  - new deterministic cases prove both follow-through triggers:
    - new eligible locker-zone gear requeues a worker after the baseline no-op pass and services once the existing cooldown expires
    - losing managed locker gear requeues the affected worker and re-equips from the locker once the existing cooldown expires
  - deterministic logs now show explicit `state-dirty` queue events in addition to the existing `before` / `plan` / `after` locker summaries
- prior broader runtime baseline remains valid for this same downtime path:
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
- Locker Zone v1 is covered deterministically on top of the already-proved runtime service path
- the dirty-trigger follow-through evidence closes the last V1 chunk rather than defining the whole lane by itself
- no extra startup/live rerun was needed for that final chunk because the changed code only adds queue-dirty detection around the same already-proved downtime/service path
- this locker V1 slice is checkpointed; do not keep revalidating it unless the locker runtime path changes again

---

## Pending probes

No standing probes for finished Locker Zone v1.

### Non-blocking Josef notes

None for finished Locker Zone v1.
If Josef later greenlights another lane, rewrite this section to match that lane instead of appending stale archaeology.

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
