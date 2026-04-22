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

### Bandit overmap-proof rule

For the remaining bandit AI proof packets, single-turn deterministic checks are **not** enough by themselves.
The honest bar now includes real overmap-side multi-turn scenario proof, up to `500` turns where needed, with explicit per-scenario goals and tuning metrics.

---

## Current relevant evidence

Active probe obligation: `Bandit + Basecamp playtest kit packet v0`.
The helper-packaging question and the first-pass readability question are both now closed enough to move on. The missing truth is now repeatability, screen-first artifact clarity, cleanup, and fast-reload playtest footing on top of the proven helper paths.

### Active lane - Bandit + Basecamp playtest kit packet v0

- canonical packet: `doc/bandit-basecamp-playtest-kit-packet-v0-2026-04-22.md`
- starting helper/readability footing now comes from the closed helper and first-pass packets:
  - `tools/openclaw_harness/scenarios/bandit.basecamp_named_spawn_mcw.json`
  - `tools/openclaw_harness/scenarios/bandit.basecamp_first_pass_readability_mcw.json`
  - fresh current-build helper proof under `.userdata/dev-harness/harness_runs/20260422_132353/`
  - fresh current-build first-pass readability proof under `.userdata/dev-harness/harness_runs/20260422_144921/`
- fresh harness-side repeatability/reporting/cleanup proof now lives under `.userdata/dev-harness/harness_runs/20260422_151547/`, via `python3 tools/openclaw_harness/startup_harness.py repeatability bandit.basecamp_named_spawn_mcw`
  - the packaged helper reran three times on the same McWilliams footing
  - `filter_bandit_template.after` matched the expected filtered-bandit menu lines on all three runs
  - each probe report now records a `cleanup` block, and all three repeatability runs exited with `cleanup.status = terminated`
  - the run stayed runtime-current (`version_matches_runtime_paths = true`) while `version_matches_repo_head = false`, which is expected here because the tree changed only in harness/scenario files and no fresh tiles rebuild was required for this slice
  - the new `repeatability.summary.txt` / `repeatability.report.json` surface is screen-first enough to show the remaining rough edge honestly: the right-panel anger line only OCR-matched on one of the three runs, so the operator can see capture sensitivity directly instead of pretending the generic `inconclusive_no_new_artifacts` verdict settled it
- honest missing proof now: one small fast-reload fixture/save pack plus a named scenario family on top of this cleaned-up helper footing
- this packet is primarily harness/helper/fixture work; live proof still matters, but another bare first-pass readability rerun is not the missing evidence class anymore

### Latest closed lane - Bandit + Basecamp first-pass encounter/readability packet v0

- canonical packet: `doc/bandit-basecamp-first-pass-encounter-readability-packet-v0-2026-04-22.md`
- fresh current-build proof lives under `.userdata/dev-harness/harness_runs/20260422_144921/`
- the bounded live readability helper for this packet is now `tools/openclaw_harness/scenarios/bandit.basecamp_first_pass_readability_mcw.json`
- the packet used the already-closed helper seam and then captured immediate plus short-horizon screen artifacts (`immediate_{full,local,right_panel}` and `after_8_turns_{full,local,right_panel}`) instead of pretending the generic artifact verdict could answer the product question by itself
- immediate on-screen read is weak: the spawn mostly reads as one more purple nearby-NPC name plus old movement/saving clutter, so the player does not get a strong first-pass spatial read on why this new person is the threat
- eight turns later the encounter is unmistakably real and dangerous, but still mostly through the right-panel log rather than clean spatial staging: `Heath Griffith, Bandit gets angry!`, taunts, safe-mode survivor ping, gunfire, and the deaths of Katharina Leach and Robbie Knox all land in the log
- honest verdict: the encounter is mechanically present and dangerous enough to justify more follow-through, but first-pass readability is weak/confusing enough that the correct next step is playtest-surface polish rather than another setup/readability feasibility lap
- no deterministic tests were added or rerun because this packet stayed on live probe / artifact work only; fresh live proof came from the current tiles binary and `python3 tools/openclaw_harness/startup_harness.py probe bandit.basecamp_first_pass_readability_mcw`

### Latest closed lane - Live bandit + Basecamp playtest packaging/helper packet v0

- canonical packet: `doc/live-bandit-basecamp-playtest-packaging-helper-packet-v0-2026-04-22.md`
- landed helper: `tools/openclaw_harness/scenarios/bandit.basecamp_named_spawn_mcw.json`
- fresh current-build proof lives under `.userdata/dev-harness/harness_runs/20260422_132353/`
- current-build proof is honest: `window_title = Cataclysm: Dark Days Ahead - 7ab535f0c7`, `version_matches_repo_head = true`, and `version_matches_runtime_paths = true`
- reviewer-readable helper artifacts now exist directly on the run path:
  - `filter_bandit_template.after.{png,screen_text.txt}` showing the filtered `bandit` menu entries
  - `post_spawn_settle.after.{png,screen_text.txt}` showing `Joshua Wilkes, Bandit gets angry!`
- forced rebuild was used here not because the packet needed gameplay-code validation, but because an earlier stale tiles binary made the first helper proof carry an avoidable asterisk before handoff
- honest remaining rough edges: the packaged path still uses the named-NPC debug spawn surface, still depends on the captured McWilliams fixture, and the generic probe artifact verdict is still less useful than the screen/OCR companions for this seam
- no deterministic tests were added or rerun because this packet landed as harness/helper/docs work only

### Latest closed lane - Live bandit + Basecamp playtesting feasibility probe v0

- canonical packet: `doc/live-bandit-basecamp-playtesting-feasibility-probe-v0-2026-04-21.md`
- fresh current-build startup proof lives under `.userdata/dev/harness_runs/20260422_002122/`, with build head `5af2fb80d8-dirty`, `version_matches_repo_head = true`, and `version_matches_runtime_paths = true`
- bounded live restage proof lives under `.userdata/dev-harness/harness_runs/20260422_002329/`, showing named NPC debug spawn of hostile `Stefany Billings, Bandit`
- honest verdict: current-build bandit + Basecamp live playtesting is practical now, but reviewer-clean packaged overmap-bandit harness footing is still absent

### Latest closed lane - Basecamp AI capability audit/readout packet v0

- canonical packet: `doc/basecamp-ai-capability-audit-readout-packet-v0-2026-04-21.md`
- honest verdict: the player-facing spoken Basecamp surface is still a narrow deterministic craft-request plus board/job router, while the richer prompt-shaped layer is mostly snapshot/planner packaging rather than core spoken command interpretation
- no compile or live probe was required because this was a current-tree capability readout, not a fresh runtime behavior claim

### Latest closed lane - Locker Zone V3

- canonical packet: `doc/locker-zone-v3-reopen-packet-v0-2026-04-21.md`
- honest verdict: hot/cold outerwear bias, hot/cold legwear bias, moderate-temperature seasonal dressing, rainy moderate-weather rainproof preference, and bounded worker-specific wardrobe preservation now all land on the same real locker seam
- current focused validation: `make -j4 tests` and `./tests/cata_test "[camp][locker]"`

### Closed bandit readiness train

Use the auxiliary packet docs for the detailed proof shape.
The canonical closed packets are:
- `doc/bandit-overmap-local-handoff-interaction-packet-v0-2026-04-21.md`
- `doc/bandit-elevated-light-and-z-level-visibility-packet-v0-2026-04-21.md`
- `doc/bandit-overmap-benchmark-suite-packet-v0-2026-04-21.md`
- `doc/bandit-weather-concealment-refinement-packet-v0-2026-04-21.md`
- `doc/bandit-overmap-local-pressure-rewrite-packet-v0-2026-04-21.md`
- `doc/bandit-long-range-directional-light-proof-packet-v0-2026-04-21.md`
- `doc/bandit-bounded-scout-explore-seam-v0-2026-04-21.md`
- `doc/bandit-repeated-site-revisit-behavior-packet-v0-2026-04-21.md`
- `doc/bandit-scoring-refinement-seam-v0-2026-04-21.md`
- `doc/bandit-moving-bounty-memory-seam-v0-2026-04-21.md`

Current honest summary:
- the playback/proof surface is now checkpointed through the handoff, visibility, benchmark, weather, pressure-rewrite, long-range-light, scout/explore, moving-memory, scoring, and repeated-site packets
- the bandit proof family has current deterministic coverage on the tree, with `./tests/cata_test "[bandit]"` as the broad filtered confidence pass when code in that family changes
- no further rerun is warranted until a fresh greenlight or contradictory evidence appears

---

## Pending probes

A live probe is greenlit now.

- Do **not** rerun the first-pass readability packet ceremonially now that its product question has an honest answer.
- The current live question is fixture-backed playtest-kit quality on top of the now-proved repeatability/reporting/cleanup seam.
- Prefer the named helper/readability scenarios over retyping the debug-menu choreography from memory.
- Use `python3 tools/openclaw_harness/startup_harness.py repeatability bandit.basecamp_named_spawn_mcw` when the question is whether the packaged helper still restages cleanly across reruns before or after fixture work.
- If the kit work surfaces a real blocker, name it concretely instead of laundering operator annoyance into vague harness vibes.

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

### Current live playtest-packaging/helper seam
- `make -B -j4 TILES=1 cataclysm-tiles`
- `python3 tools/openclaw_harness/startup_harness.py probe bandit.basecamp_named_spawn_mcw`

### Closed first-pass encounter/readability seam
- `make -B -j4 TILES=1 cataclysm-tiles`
- `python3 tools/openclaw_harness/startup_harness.py probe bandit.basecamp_first_pass_readability_mcw`

### Current active playtest-kit footing
- `make -B -j4 TILES=1 cataclysm-tiles`
- `python3 tools/openclaw_harness/startup_harness.py probe bandit.basecamp_named_spawn_mcw`
- `python3 tools/openclaw_harness/startup_harness.py repeatability bandit.basecamp_named_spawn_mcw`
- `python3 tools/openclaw_harness/startup_harness.py probe bandit.basecamp_first_pass_readability_mcw`
- preserve the screen/OCR artifacts that show repeatability, readability, cleanup behavior, and later fixture-backed variants

## Local build caveat

On this Mac, treat top-level `make -j4 tests` as the reliable path for a fresh `cata_test`.
Avoid treating `make -C tests cata_test` as authoritative here; it has been a repeated source of toolchain/stale-build nonsense.
Also: if you actually need a fresh tiles binary, use `make -j4 TILES=1 cataclysm-tiles`; plain `make cataclysm-tiles` is not an honest rebuild path here.
