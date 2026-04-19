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

### Organic bulletin-board speech polish

Current honest state:
- this lane became active after the locker surface/control follow-through reclosed honestly, and the first fresh 2026-04-19 audit narrowed the remaining visible seam instead of hand-waving at it:
  - ordinary spoken board/job replies were still appending request-id chatter like `(#7)`
  - board-status parsing still missed some obvious organic asks like `what needs making`, `what needs doing`, and `got any craft work`
  - the raw structured `show_board` / `show_job` / `job=` payload leak itself still stayed fixed
- that narrow cleanup is now landed in code with deterministic coverage on the current dirty tree:
  - `src/faction_camp.cpp` stops ordinary spoken board/job barks from echoing request ids
  - `parse_heard_camp_status_query()` now accepts the added organic board-status asks above
  - `./tests/cata_test "[camp][basecamp_ai]"` passed after a fresh `make -j4 tests`
- proportional live runtime proof now exists on the rebuilt current tiles binary:
  - `make -j4 TILES=1 cataclysm-tiles` passed on 2026-04-19
  - `.userdata/dev-harness/harness_runs/20260419_154244/` used the real `assign_nearby_npc_to_camp_dialog` seam, not the false guard/hold-only setup
  - the visible screen packet for `what needs making` shows `Katharina Leach says, "Board's got 1 live and 1 old - 1 x bandages."`
  - the same visible reply stays free of `(#7)`, `req_0`, `req_1`, and similar machine glue
  - matching artifacts show `camp routing check ... uses_basecamp=yes`, `camp heard Katharina Leach (2)`, and `player utterance: what needs making`
- Robbie still chimed in on that McWilliams fixture as ordinary follower crosstalk after Katharina answered; treat that as existing routing noise, not as proof that the closed raw-payload leak regressed
- do not reopen the older board-routing/raw-payload work unless a new probe actually shows those claims were oversold

### Recently closed, do not casually reopen

- Locker surface/control is now reclosed on both deterministic and live footing:
  - deterministic proof still covers locker policy persistence and sorting skip behavior on `CAMP_LOCKER`
  - live run `.userdata/dev-harness/harness_runs/20260419_141422/` created `Basecamp: Locker`, renamed it to `Probe Locker`, used the single-`Esc` -> save-prompt -> `Y` closeout, and reopened Zone Manager with `Probe Locker` still present
  - no type-mismatch popup or related stderr/debug failure surfaced on that live packet

### Meaning

- the missing proof for this narrowed slice is no longer live board-status behavior; that packet now exists on the rebuilt current runtime with screen and artifact evidence kept separate
- the next honest move is closeout/checkpoint hygiene, not more locker truth and not more blind board archaeology

---

## Pending probes

- none by default for this slice
- only run another live packet if checkpoint prep specifically needs explicit current-runtime job-followup wording, or if a fresh visible seam appears

---

## Reusable commands

Use these when they are actually the missing evidence, not as ritual.

### Startup/load smoke for later live board proof
- `python3 tools/openclaw_harness/startup_harness.py start --profile dev --world 'Sandy Creek'`

### Organic board-speech live proof
- `python3 tools/openclaw_harness/startup_harness.py probe basecamp.organic_board_speech_probe_mcw`

### Recently used locker closeout probe
- `python3 tools/openclaw_harness/startup_harness.py probe locker.zone_manager_save_probe_mcw`

## Local build caveat

On this Mac, treat top-level `make -j4 tests` as the reliable path for a fresh `cata_test`.
Avoid treating `make -C tests cata_test` as authoritative here; it has been a repeated source of toolchain/stale-build nonsense.
Also: if you actually need a fresh tiles binary, use `make -j4 TILES=1 cataclysm-tiles`; plain `make cataclysm-tiles` is not an honest rebuild path here.
