# Minimal dev freeze checklist (2026-04-09)

Use this only after Josef's last debug round.
If the round finds a real blocker, stop and fix that instead of pretending a checklist is magic.

## Preconditions
- `dev` still reflects current reviewed truth
- no new feature lane got opened out of boredom
- Package 4 and Package 5 stayed closed during freeze prep

## Evidence already prepared
- rebuilt current HEAD on this Mac:
  - `build_logs/freeze_prep_tests_20260409.log`
  - `build_logs/freeze_prep_tiles_20260409.log`
- filtered deterministic checks passed:
  - `build_logs/freeze_prep_smart_zone_tests_20260409.log`
  - `build_logs/freeze_prep_basecamp_ai_tests_20260409.log`
- live Smart Zone rerun packet exists:
  - `.userdata/dev-harness/harness_runs/20260409_140439/`
- live assigned-camp board/log rerun packet exists:
  - `.userdata/dev-harness/harness_runs/20260409_140655/`
- Josef reviewer packet exists:
  - `doc/josef-final-debug-round-packet-2026-04-09.md`

## Freeze decision checklist
- [ ] Josef's final debug round does not disprove the current packet
- [ ] No fresh crash, debug-popup spam, or visible player-facing board/log leak appears
- [ ] Smart Zone Manager v1 still looks sane enough to ship as the current narrow v1
- [ ] `Plan.md`, `TODO.md`, `SUCCESS.md`, and `TESTING.md` still match each other
- [ ] Working tree is clean / explainable before the freeze step

## If the checklist stays green
- freeze `dev -> master`
- describe the runtime-visible delta narrowly:
  - Smart Zone Manager v1 landed
  - assigned-camp board/log fix was rechecked and stayed good
  - no Package 4 / 5 widening was opened during freeze prep
- carry the tiny artifact packet, not a giant screenshot landfill

## If the checklist goes red
- do **not** open new future lanes to feel productive
- fix only the blocker or explicitly postpone the freeze
- keep the packet honest
