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

### Plan/Aux pipeline helper

Current honest state:
- the suspicion-first audit is now closed honestly: the repo had stable canon-role docs and recurring aux patterns, but no repo-local helper for contract preview, correction merge, canon/snippet packaging, or bounded in-place patching
- the current bounded helper path now exists at `tools/plan_aux_pipeline_helper.py`
- current landed command set is:
  - `schema` prints the contract shape
  - `show` prints repo config, contract preview, and patch matrix before edits
  - `merge-corrections` merges an explicit corrections overlay into the reviewed contract
  - `emit` writes reviewer-visible aux/canon snippet files from the same classified contract
  - `apply` patches the auxiliary doc plus known existing `Plan.md` / `TODO.md` / `SUCCESS.md` / `TESTING.md` anchors in place, while still failing honestly when a requested heading does not already exist
- focused validation passed for this tooling slice via:
  - `python3 -m py_compile tools/plan_aux_pipeline_helper.py`
  - `python3 tools/plan_aux_pipeline_helper.py schema`
  - `python3 tools/plan_aux_pipeline_helper.py show /tmp/plan_aux_helper_spec.json`
  - `python3 tools/plan_aux_pipeline_helper.py emit /tmp/plan_aux_helper_spec.json --out-dir /tmp/plan_aux_helper_emit --force`
  - `python3 tools/plan_aux_pipeline_helper.py apply /tmp/plan_aux_helper_spec.json --root /tmp/plan_aux_helper_repo`
- the main canon-patching path is now honestly real for known headings; the remaining open slice is optional handoff generation from the same classified contract

### Recently closed, do not casually reopen

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

- the missing evidence for the active lane is not another locker rerun or compile ritual
- the main canon-patching path is now proven narrowly enough, while the now-closed combat packet stays closed unless a fresh contradiction appears
- the next honest decision is whether optional Andi handoff generation deserves to be the final helper slice

---

## Pending probes

- decide whether optional Andi handoff generation is worth landing as the final helper slice
- if that slice lands, validate it with the narrowest command-level proof on a sample spec instead of broad repo builds

---

## Reusable commands

Use these when they are actually the missing evidence, not as ritual.

### Narrow locker deterministic pass after a code slice lands
- `make -j4 tests`
- `./tests/cata_test "[camp][locker]"`

### Future helper validation only if code actually lands
- use the narrowest test or script-level proof that matches the touched helper entry point
- do not rebuild game binaries for docs-only helper audit work

### Fresh tiles rebuild only if a later combat-policy handoff really needs live proof
- `make -j4 TILES=1 cataclysm-tiles`

## Local build caveat

On this Mac, treat top-level `make -j4 tests` as the reliable path for a fresh `cata_test`.
Avoid treating `make -C tests cata_test` as authoritative here; it has been a repeated source of toolchain/stale-build nonsense.
Also: if you actually need a fresh tiles binary, use `make -j4 TILES=1 cataclysm-tiles`; plain `make cataclysm-tiles` is not an honest rebuild path here.
