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

### Locker Zone V1 checkpoint baseline
Latest relevant agent-side locker packet:
- forced-rebuild audit on committed HEAD `dd4faafe32`
  - `make -B -j4 tests`
  - `./tests/cata_test "[camp][locker]"`
  - passed at `121 assertions in 10 test cases`
- the targeted suite covers the bundled V1 close-out shape:
  - locker policy serialization / save-load
  - representative locker item classification
  - locker-zone candidate gathering and reservation filtering
  - locker loadout planning and disabled-category behavior
  - service/equip/upgrade/return behavior
  - one-worker-at-a-time queue sequencing
  - new eligible locker gear requeues a worker after a baseline no-op pass
  - losing managed locker gear requeues the affected worker
  - transient assigned-camp downtime rehydration
- prior broader runtime baseline remains valid for this same downtime/service path:
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
- Locker Zone V1 stays checkpointed because the bundled V1 task set is backed by deterministic proof plus proportional runtime proof
- this packet is the baseline to preserve while working on V2
- if V2 work breaks any bundled V1 claim, reopen V1 instead of waving it away

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

### Active queue — Locker Zone V2

1. define and test the minimal V2 ranged-readiness contract
   - up to two compatible magazines when useful
   - reload support from locker-zone ammo supply
   - no seasonal/fashion nuance sneaking in from V3
2. add/update deterministic coverage for the V2 magazine / reload behavior
3. rerun `./tests/cata_test "[camp][locker]"` after the V2 slice lands
4. only use startup/live smoke if the changed runtime path cannot be settled honestly by the deterministic locker suite alone

### Non-blocking Josef notes

None yet for Locker Zone V2.
If later needed, add V2/V3 notes here without turning them into blockers.

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
