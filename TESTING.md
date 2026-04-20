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

### Active lane — Bandit perf + persistence budget probe v0

Current honest state:
- the dry-run evaluator seam now exists in `src/bandit_dry_run.{h,cpp}` and stays deliberately abstract instead of pretending full autonomous bandit behavior exists already
- the named playback seam now exists in `src/bandit_playback.{h,cpp}`, with seven stable deterministic scenarios and replay checkpoints at `tick 0`, `tick 5`, `tick 20`, and `tick 100`
- deterministic coverage now exists in `tests/bandit_playback_test.cpp` for idle fallback, smoke investigation/cool-off, corridor stalking, forest-vs-town pivoting, moving-carrier attachment, split-route selection, city-edge peel-off, and report rendering
- validation passed proportionally for the landed evaluator + playback footing via `make -j4 tests` and `./tests/cata_test "[bandit]"`
- the missing evidence is now measurement: repeatable cost, visible churn, and first-pass save-size pressure on those named scenarios

### Recently closed, do not casually reopen

- Bandit scenario fixture + playback suite v0 is now honestly checkpointed:
  - `src/bandit_playback.{h,cpp}` provides seven stable named deterministic scenarios on top of the bounded dry-run evaluator instead of pretending a broader world simulator already exists
  - `run_scenario()` replays those cases at `tick 0`, `tick 5`, `tick 20`, and `tick 100`, while `render_report()` gives a reviewer-readable checkpoint summary for drift questions
  - narrow deterministic validation passed via `make -j4 tests` and `./tests/cata_test "[bandit]"`
- Bandit evaluator dry-run seam v0 is now honestly checkpointed:
  - `src/bandit_dry_run.{h,cpp}` provides the bounded abstract evaluator, candidate board, visible scoring ladder, threat gating, and downstream `no_path` rejection without smuggling in playback or persistence architecture
  - `render_report()` provides the first inspectable explanation surface instead of ghost-hunting through debugger soup
  - narrow deterministic validation passed via `make -j4 tests` and `./tests/cata_test "[bandit][dry_run]"`
- Plan status summary command is now honestly checkpointed:
  - `tools/plan_status_summary.py` provides the deliberately small read-only command seam for current canon
  - thin/contradictory handling is covered by the built-in self-test plus the live thin-canon warning on the current `Hackathon feature lanes` heading
  - validation stayed proportional: command-level proof only, no build or harness ritual
- Bandit concept formalization follow-through is now honestly checkpointed:
  - Package 3, micro-item 31 (`Invariants and non-goals packet`) is now landed, so the full 3-package / 31-micro-item packet finally has explicit red-line invariants/non-goals on top of its starter numbers and worked scenarios
  - the follow-through now closes as docs-only control-law cleanup inside the parked bandit chain, not as implementation greenlight
  - validation stayed honest for the slice: docs-only change, so no compile or harness ritual was needed
- Plan/Aux pipeline helper is now honestly checkpointed:
  - `tools/plan_aux_pipeline_helper.py` still keeps canon patching bounded to known headings, but `emit` can now also produce downstream `andi.handoff.md` output from the same validated classified contract
  - narrow validation passed via `python3 -m py_compile tools/plan_aux_pipeline_helper.py`, `schema`, `show`, `emit`, emitted handoff review, and `apply` on a temp repo copy
- Combat-oriented locker policy is now honestly checkpointed:
  - the final closure audit found one real remaining deterministic gap, namely end-to-end service proof for the newly explicit common combat-support slots
  - that gap is now closed by `camp_locker_service_equips_common_combat_support_slots`, which proves real locker service equips gloves, dust mask, tool belt, and holster from `CAMP_LOCKER` stock
  - focused deterministic validation passed on the current tree via `make -j4 tests`, `./tests/cata_test "camp_locker_service_equips_common_combat_support_slots"`, and `./tests/cata_test "[camp][locker]"`
  - the filtered locker suite still covers the previously suspicious safety seams this packet could have broken, including great-helm classification, ballistic body-armor comparisons, holster-vs-pants cleanup behavior, weird-garment preservation, weather-sensitive outerwear/legwear handling, and full-body suit protection logic
- Organic bulletin-board speech polish is now reclosed on both deterministic and live footing:
  - `make -j4 tests` plus `./tests/cata_test "[camp][basecamp_ai]"` passed for the widened organic status parsing and cleaned spoken bark
  - live run `.userdata/dev-harness/harness_runs/20260419_154244/` used the real camp-assignment seam, asked `what needs making`, and showed `Board's got 1 live and 1 old - 1 x bandages.` with no visible request-id glue
  - Robbie's same-packet follower crosstalk stays separate routing noise, not a reason to reopen the closed machine-speech seam
- Locker surface/control is also reclosed on both deterministic and live footing:
  - deterministic proof still covers locker policy persistence and sorting skip behavior on `CAMP_LOCKER`
  - live run `.userdata/dev-harness/harness_runs/20260419_141422/` created `Basecamp: Locker`, renamed it to `Probe Locker`, used the single-`Esc` -> save-prompt -> `Y` closeout, and reopened Zone Manager with `Probe Locker` still present
  - no type-mismatch popup or related stderr/debug failure surfaced on that live packet

### Meaning

- the evaluator seam and the named playback seam are no longer the missing evidence class, they are the substrate for the new active lane
- the missing evidence is now repeatable cost / churn / save-budget measurement on those scenarios, not more playback theater
- broader optimization or persistence architecture still stays out of scope until this measurement slice says it is actually needed

---

## Pending probes

- on the first perf/persistence slice, run the narrowest honest compile/test for the touched seam only
- measure evaluator-loop cost on the named playback scenarios and keep the readout repeatable
- expose obvious churn counters or similarly cheap visibility for candidate generation / scoring / path-check style work
- estimate save-size impact from the bounded bandit state shape the current v0 docs actually claim
- compare at least the shorter and longer playback horizons so cost drift is visible instead of flattened into one number
- do **not** jump to harness/live play unless the measurement seam honestly stops being answerable through deterministic fixtures

---

## Reusable commands

Use these when they are actually the missing evidence, not as ritual.

### Narrow locker deterministic pass after a code slice lands
- `make -j4 tests`
- `./tests/cata_test "[camp][locker]"`

### Plan status summary command
- `python3 tools/plan_status_summary.py --self-test`
- `python3 tools/plan_status_summary.py /plan`
- `python3 tools/plan_status_summary.py /plan active`
- `python3 tools/plan_status_summary.py /plan greenlit`
- `python3 tools/plan_status_summary.py /plan parked`

### Bandit dry-run evaluator seam foundation
- `make -j4 tests`
- `./tests/cata_test "[bandit][dry_run]"`

### Fresh tiles rebuild only if a later combat-policy handoff really needs live proof
- `make -j4 TILES=1 cataclysm-tiles`

## Local build caveat

On this Mac, treat top-level `make -j4 tests` as the reliable path for a fresh `cata_test`.
Avoid treating `make -C tests cata_test` as authoritative here; it has been a repeated source of toolchain/stale-build nonsense.
Also: if you actually need a fresh tiles binary, use `make -j4 TILES=1 cataclysm-tiles`; plain `make cataclysm-tiles` is not an honest rebuild path here.
