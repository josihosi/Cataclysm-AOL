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

## Current baseline evidence

### Basecamp bark baseline
Latest relevant agent-side baseline for the current bark pass:
- `./tests/cata_test "[camp][basecamp_ai]"` passed at `269 assertions in 1 test case`
- `python3 tools/openclaw_harness/startup_harness.py start --profile dev --world 'Sandy Creek'` passed cleanly
- current relevant harness artifact: `.userdata/dev/harness_runs/20260405_014335`

Meaning:
- current bark tree compiles and starts
- current remaining question is human feel, not technical survival
- this packet can wait while agent-side work continues elsewhere

### Locker Zone v1 baseline
Latest relevant agent-side baseline for the now-checkpointed locker tree:
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

### LLM-side board snapshot baseline
Latest relevant baseline for the newly active board-snapshot lane:
- `./tests/cata_test "[camp][basecamp_ai]"`
  - passed at `269 assertions in 1 test case` after syncing the shipped `basecamp_board_handoff_snapshot.txt` template with `{{planning_snapshot}}`
- current shipped prompt template now begins with:
  - `board=show_board`
  - `{{planning_snapshot}}active={{active_count}}`

Meaning:
- the code/tests and the on-disk prompt template now agree about the planning-snapshot placeholder
- **missing evidence is the real structured / LLM-side routing proof, not another compile or startup ritual**

---

## Active validation target — LLM-side board snapshot path

### Current truth
The planning-snapshot placeholder is back in the shipped board handoff prompt, and the current deterministic `basecamp_ai` packet is green.
The missing question is now routing: where the richer board handoff snapshot actually enters the structured / LLM-side path, and whether that route stays separate from the short spoken board bark.

### Required next validation
Capture the smallest honest packet that proves the routing claim:
1. where `show_board` / board-handoff data is emitted for the structured / LLM-side path
2. that the board handoff snapshot can carry `planner_move` / overmap context on that path
3. that the human-facing spoken board bark remains concise rather than dumping the full handoff snapshot

Prefer deterministic or artifact/log evidence first.
Do **not** substitute another broad startup rerun unless the code actually changes in a way that makes startup the missing evidence class.

---

## Pending Josef checks — Basecamp bark feel pass

Josef is only needed here for tone/feel judgment on the current bark tree.
Current packet:
- `show me the board` / `what's on the board`
- `status of bandages`
- `craft 5 bandages`
- `clear request #...`

Expected shape:
- concise spoken board bark
- subject-first status bark with trailing `(#id)` detail where needed
- clear blocked-craft wording without spreadsheet sludge
- clear/archive wording that sounds like camp speech, not office archiving

Until the code changes again, do not keep revalidating this technically just because the human answer is not back yet.
If another Josef-only packet appears soon, batch them.

---

## Reusable commands

Use these when they are actually the missing evidence, not as ritual.

### Narrow deterministic locker check
- `./tests/cata_test "[camp][locker]"`

### Narrow deterministic Basecamp bark check
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
