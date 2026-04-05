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
Latest relevant agent-side baseline for the current locker tree:
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
- the current binary compiles, relinks, starts cleanly, and now has one real downtime-driven live locker packet on the current binary
- **the remaining work for Locker Zone v1 is repo hygiene / checkpointing, not more technical proof unless the code changes again**

---

## Active validation target — Locker Zone v1

### Current truth
The required live downtime proof now exists.
The useful live packet on the current binary is the Bruna Priest pass recorded in current `debug.log`, where the actual downtime queue path removed duplicate managed gear and dropped the displaced items into locker storage.
That clears the missing evidence class that used to block this target.

### Required next validation
No additional live probe is currently required.
If the upcoming checkpoint split changes locker behavior, rerun only the narrowest honest evidence for the touched slice:
- `./tests/cata_test "[camp][locker]"` for code/test changes
- another live locker probe only if the split or cleanup actually changes the live path or invalidates the current packet

### Repo hygiene requirement for this target
Checkpoint or split the locker work per `COMMIT_POLICY.md` before piling on more unrelated changes.
The current locker tree is already large enough that repo hygiene is part of the target now, not optional garnish.

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
