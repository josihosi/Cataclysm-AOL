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

### Post-Locker-V1 Basecamp follow-through close-out
Latest relevant agent-side packet:
- committed HEAD `64f2bebc85`
  - `make -j4 tests`
  - `./tests/cata_test "[camp][basecamp_ai]"`
  - passed at `320 assertions in 1 test case`
- the targeted suite now covers the closed structured follow-through loop exposed by the board snapshot itself:
  - structured `show_board` still emits the richer board snapshot with planning context
  - structured `job=<id>` now follows the board-emitted `next=` token with an updated structured job snapshot instead of dropping back to spoken bark
  - structured `delete_job=<id>` now follows the archived board-emitted `next=` token with a refreshed structured board snapshot after the clear
  - the short spoken board bark stays separate from the structured path
- earlier live/runtime artifact proof for `show_board` still stands as the current baseline:
  - `make -j4 TILES=1 cataclysm-tiles`
  - launched `./cataclysm-tiles --userdir .userdata/dev --world 'Sandy Creek'`
  - used `Shift+C`, then `bshow_board`
  - fresh `config/llm_intent.log` append showed the fenced structured packet with `camp board reply ...`, `heard=show_board`, `reply_begin`, payload, `reply_end`

Meaning:
- the previously active Basecamp follow-through queue is closed for now
- deterministic coverage now reaches the board-emitted per-job `next=` actions, not only the initial `show_board` inspection token
- no broader rebuild or live smoke rerun was needed for this slice because the change stayed local to the camp request router/test surface already covered by the targeted suite
- if Josef later wants more Basecamp prompt work, use this packet as the current deterministic baseline

---

## Pending probes

No active probe queue right now.

### Non-blocking Josef notes

None.
If Josef later wants extra confidence on the structured follow-through artifacts, batch a live `DEBUG_LLM_INTENT_LOG` probe for `job=` / `delete_job=` with the next runtime-facing Basecamp slice instead of doing a ceremonial standalone rerun.

---

## Reusable commands

Use these when they are actually the missing evidence, not as ritual.

### Narrow deterministic Basecamp bark / routing check
- `./tests/cata_test "[camp][basecamp_ai]"`

### Narrow deterministic locker check
- `./tests/cata_test "[camp][locker]"`

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
