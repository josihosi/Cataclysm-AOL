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
- current committed HEAD `7c2bf09ec1`
  - `make -j4 tests`
  - `./tests/cata_test "[camp][basecamp_ai]"`
  - passed at `296 assertions in 1 test case`
- the targeted suite still covers the real `handle_heard_camp_request` router path, not only helper builders
- it still proves the existing route split:
  - structured `show_board` emits the richer handoff snapshot
  - spoken `show me the board` stays on the concise human-facing bark path
  - fenced board/job reply packets still use `reply_begin` / `reply_end`
- new cleanup-specific proof now also shows the packaging seam explicitly:
  - the stable deterministic board body (`active` / `archived` / `job=...`) is reusable on its own
  - the `planner_move` / `overmap` block is now only a prefixed layer added by the origin-aware `show_board` path
  - so the current deterministic board snapshot packages cleanly without baking prompt-planning context into the core board template
- earlier broader runtime artifact proof remains valid as the live reference:
  - `make -j4 TILES=1 cataclysm-tiles`
  - launched `./cataclysm-tiles --userdir .userdata/dev --world 'Sandy Creek'`
  - used `Shift+C`, then `bshow_board`
  - fresh `config/llm_intent.log` append showed the fenced structured packet with `camp board reply ...`, `heard=show_board`, `reply_begin`, payload, `reply_end`
- earlier forced rebuild audit on committed HEAD `dd4faafe32` still matters as stale-binary defense:
  - `make -B -j4 tests`
  - `./tests/cata_test "[camp][basecamp_ai]"`
  - passed on the rebuilt current-head binary instead of the older stale-banner mismatch

Meaning:
- the board-routing proof slice stays closed as the baseline
- the upstream deterministic cleanup slice is now agent-validated too
- use this packet as the deterministic + live reference while working through the remaining broader prompt follow-through slice
- rerun broader runtime smoke only if the next sub-slice actually changes the live route/output class again

### Locker Zone v1 checkpoint
Latest relevant agent-side locker packet:
- `make -j4 tests`
- `./tests/cata_test "[camp][locker]"`
  - passed at `121 assertions in 10 test cases` on dirty `ab9cd121f8`
  - new deterministic cases prove both follow-through triggers:
    - new eligible locker-zone gear requeues a worker after the baseline no-op pass and services once the existing cooldown expires
    - losing managed locker gear requeues the affected worker and re-equips from the locker once the existing cooldown expires
  - deterministic logs now show explicit `state-dirty` queue events in addition to the existing `before` / `plan` / `after` locker summaries
- later audit on current committed HEAD `dd4faafe32`
  - `make -B -j4 tests`
  - `./tests/cata_test "[camp][locker]"`
  - passed again at `121 assertions in 10 test cases`
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
- the done-marked locker baseline also survives a forced rebuild on current committed HEAD
- no extra startup/live rerun was needed for that final chunk because the changed code only adds queue-dirty detection around the same already-proved downtime/service path
- this locker V1 slice is checkpointed; do not keep revalidating it unless the locker runtime path changes again

---

## Pending probes

### Active queue — post-Locker-V1 Basecamp follow-through

1. **Broader LLM-side board prompt follow-through**
   - define the exact next structured extension beyond `show_board` before writing code
   - add/update the narrow deterministic coverage for that new route
   - rerun `./tests/cata_test "[camp][basecamp_ai]"`
   - use startup/live smoke only if the changed runtime path cannot be settled honestly by deterministic coverage alone

### Non-blocking Josef notes

None yet for the active Basecamp follow-through queue.
If a later sub-slice needs Josef judgment, add it here as a note rather than turning it into a blocker.

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
Also: if you actually need a fresh tiles binary, use `make -j4 TILES=1 cataclysm-tiles`; plain `make cataclysm-tiles` is not an honest rebuild path here.
