# Josef final debug-round packet (2026-04-09)

This is the short honest packet for Josef's last debug round before freezing `dev`.
The goal is not to discover exciting new work. The goal is to make the final pass easy and to make `dev` easy to freeze if the round stays clean.

## What changed since the last stable point

Runtime-visible change since the last stable point:
- **Smart Zone Manager v1** landed on `dev` as a one-shot Basecamp auto-layout helper.

What did **not** newly land in this freeze-prep pass:
- no Package 4 work
- no Package 5 work
- no new locker/basecamp behavior widening
- no new bulletin-board speech polish work

What this freeze-prep pass did instead:
- rebuilt current HEAD (`fce15a6c5a`)
- reran the Smart Zone Manager deterministic + live packet
- reran the assigned-camp board/log leak deterministic + live packet
- cleaned the canon so `Plan.md`, `TODO.md`, `SUCCESS.md`, and `TESTING.md` all tell the same pre-freeze story

## Fresh evidence packet

### Rebuild + deterministic checks
- `make -j4 tests` -> `build_logs/freeze_prep_tests_20260409.log`
- `make -j4 TILES=1 cataclysm-tiles` -> `build_logs/freeze_prep_tiles_20260409.log`
- `./tests/cata_test "[zones][smart_zone][basecamp]"` -> `build_logs/freeze_prep_smart_zone_tests_20260409.log`
  - result: `All tests passed (2825 assertions in 4 test cases)`
- `./tests/cata_test "[camp][basecamp_ai]"` -> `build_logs/freeze_prep_basecamp_ai_tests_20260409.log`
  - result: `All tests passed (332 assertions in 1 test case)`

### Smallest useful live artifacts
- Smart Zone prompt screenshot:
  - `.userdata/dev-harness/harness_runs/20260409_140439/confirm_zone_end.after.png`
- Smart Zone saved/reopened layout screenshot:
  - `.userdata/dev-harness/harness_runs/20260409_140439/reopen_zones_manager_for_layout_capture.after.png`
- Assigned-camp board/job reply screenshot:
  - `.userdata/dev-harness/harness_runs/20260409_140655/wait_for_job_followup_reply.after.png`
- Assigned-camp internal artifact log snippet source:
  - `.userdata/dev-harness/harness_runs/20260409_140655/probe.artifacts.log`

## Exactly what Josef should test

Keep it narrow.
Two passes are enough.

### 1. Smart Zone Manager v1

#### Do
- load the current dev build on the current McWilliams/basecamp path Josef trusts
- create or edit a `Basecamp: Storage` zone
- accept the Smart Zone Manager prompt
- save, then reopen Zone Manager

#### Pass if
- the prompt appears on the real UI path
- the stamped layout appears sensible and legible
- the stamped zones persist after save/reopen
- nothing crashes, hangs, or throws fresh debug-message nonsense

#### Fail if
- no prompt appears
- the smart-zoned layout does not stamp or does not persist
- the layout is obviously nonsense on-screen
- the path crashes, misbehaves, or starts overwriting things destructively

### 2. Assigned-camp board/log path

#### Do
- talk to a **true camp-assigned** NPC, not just a nearby follower standing in camp
- ask for the board
- ask for one board follow-up / job detail

#### Pass if
- the visible in-game message log stays organic
- no raw payload like `board=show_board`, `details=show_job=1`, or `next=job=1` leaks into the player-facing log
- the assigned-camp NPC still behaves like the camp-aware path rather than generic follower chatter

#### Fail if
- raw machine payload leaks into the visible message log
- the assigned-camp actor falls back to the wrong snapshot/routing path
- the board/job follow-up visibly breaks or becomes incoherent

### Optional only if Josef feels like poking it

**Too-small-zone failure path**
- This is already implemented and freshly re-verified by deterministic tests.
- Josef does **not** need to burn time manually proving it unless he wants the product feel check.
- If he does poke it live, the only thing to care about is: does it fail cleanly instead of spraying nonsense.

## Known weird-but-non-blocking remainder

- The structured live probe still uses a player utterance like `job=1` to force the internal follow-up path. That is acceptable for the probe. It is **not** the same thing as the old raw payload leak returning.
- The assigned-camp artifact packet still records a nearby Robbie Knox response after the Katharina exchange. Annoying, yes. But in this packet it reads like chatter/recipient noise, not the old visible board/log leak.
- Organic bulletin-board speech polish is still parked future work. Do not reopen it during freeze prep unless Josef decides it is somehow a real release blocker.
- The Smart Zone live probe is a screen/UI proof, not an `llm_intent.log` proof, so its harness verdict `inconclusive_no_new_artifacts` is expected rather than alarming.

## Blunt recommendation

Josef should spend the last debug round on exactly these two surfaces:
1. Smart Zone Manager v1 real UI behavior
2. assigned-camp board/job visible message-log behavior

If both stay clean, stop digging and freeze the damn thing.
