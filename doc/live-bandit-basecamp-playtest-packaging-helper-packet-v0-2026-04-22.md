# Live bandit + Basecamp playtest packaging/helper packet v0 (2026-04-22)

Status: CHECKPOINTED / DONE FOR NOW.

## Why this packet existed

The feasibility question was already closed.
Current-build live bandit + Basecamp playtesting was practical, but the usable bandit restage seam still lived as debug-menu folklore.

So this packet stayed narrow:
- package the already-proved named-bandit restage seam into one bounded helper path
- keep real existing McWilliams/Basecamp footing instead of inventing a toy map
- leave reviewer-readable live artifacts
- say plainly what still looks ugly instead of laundering it into magic

## Landed helper path

First-class scenario:
- `tools/openclaw_harness/scenarios/bandit.basecamp_named_spawn_mcw.json`

What it packages:
- reuse `dev-harness` + world `McWilliams`
- reinstall fixture `mcwilliams_live_debug_2026-04-07` from profile `live-debug`
- open the named-NPC spawn seam with `}` -> `s` -> `p`
- filter to `bandit`
- confirm the filtered hostile spawn entry
- capture reviewer-readable cropped screenshots plus OCR text companions for the menu and post-spawn result

This keeps the operator path short enough that later live playtests can call one scenario instead of reconstructing the debug-menu dance from chat memory.

## Fresh live proof

Fresh current-build proof run:
- run dir: `.userdata/dev-harness/harness_runs/20260422_132353/`
- command: `python3 tools/openclaw_harness/startup_harness.py probe bandit.basecamp_named_spawn_mcw`
- build window title: `Cataclysm: Dark Days Ahead - 7ab535f0c7`
- `version_matches_repo_head = true`
- `version_matches_runtime_paths = true`

Reviewer-readable evidence from that run:
- filtered menu crop: `filter_bandit_template.after.png`
- filtered menu OCR: `filter_bandit_template.after.screen_text.txt`
  - `Choose NPC:`
  - `3 bandit`
  - `bandit_trader`
  - `bandit_mechanic`
  - `bandit_quartermaster`
- post-spawn crop: `post_spawn_settle.after.png`
- post-spawn OCR: `post_spawn_settle.after.screen_text.txt`
  - `Joshua Wilkes, Bandit gets angry!`

That is enough to prove the packaged helper still reaches the intended live hostile-bandit-on-current-Basecamp-footing setup on the current build.

## What remains manual or ugly

The packet is cleaner now, not magical.
Remaining rough edges are:

1. The helper still rides the debug named-NPC spawn surface underneath. The folklore is packaged, not abolished.
2. The footing still depends on the captured McWilliams/Basecamp fixture rather than a fully declarative authored world setup.
3. The generic probe artifact verdict is still a bit stupid for this seam: the meaningful proof is in cropped screenshots and OCR companions, while `probe.artifacts.log` only catches a trivial debug-log line unless future harness work grows a smarter screen-first artifact summary.
4. The probe still leaves a live game window running after capture; closing that session is still an operator cleanup step rather than part of the packaged scenario contract.

## Validation actually used

This stayed harness/helper/docs work, so validation stayed proportionate:
- forced fresh tiles rebuild: `make -B -j4 TILES=1 cataclysm-tiles`
- fresh packaged live probe on current build: `python3 tools/openclaw_harness/startup_harness.py probe bandit.basecamp_named_spawn_mcw`
- no new deterministic tests were added because no gameplay code had to change for this packet

## Out of scope, still out of scope

This packet did **not** greenlight or answer:
- new bandit mechanics
- Basecamp zoning reopen
- broader encounter/readability judgment
- a fake scenario family
- another feasibility lap

The next honest question, if Josef wants it later, is the first encounter/readability pass now that setup friction is packaged cleanly enough to stop dominating the exercise.
