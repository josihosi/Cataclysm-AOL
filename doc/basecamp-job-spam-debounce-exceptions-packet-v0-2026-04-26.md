# Basecamp job spam debounce + locker/patrol exceptions packet v0 (2026-04-26)

Status: closed / checkpointed.

Closed evidence (2026-04-27): stable-cause camp job report debounce landed for allied camp activity completion, camp request missing-tool/blocked barks, and no-progress camp request barks. Typed locker and patrol exception reports now surface named `[camp][locker]` / `[camp][patrol]` reasons once per stable state and compress repeats for 30 minutes; the cache is runtime-only, so save/load cannot hide messages forever.

Local gates:
- `git diff --check`
- `make -j4 obj/faction_camp.o obj/npcmove.o obj/basecamp.o tests/faction_camp_test.o tests LINTJSON=0 ASTYLE=0`
- `./tests/cata_test "camp_locker_missing_zone_reports_typed_exception_once"`
- `./tests/cata_test "camp_request_speech_parsing"`
- `./tests/cata_test "[camp][patrol]" && ./tests/cata_test "[camp][locker]"`

## Normalized contract

**Title:** Basecamp job spam debounce + locker/patrol exceptions packet v0

**Request kind:** Josef greenlight / active implementation package

**Summary:** Josef wants the repeated Basecamp job chatter fixed: repeated “completed the assigned task,” missing-tool, or similar job-state spam should be debounced so the message log stays readable. But locker-zone and patrol-zone behavior need explicit exceptions/typed reporting, because those systems produce important gear/guard state and should not be silently swallowed into generic cooldown mush.

## Scope

1. Identify the repeated Basecamp job message families that spam during ordinary camp work:
   - repeated completion messages;
   - repeated missing-tool / cannot-start messages;
   - repeated no-progress job-state notifications that do not add new player information.
2. Add a small debounce/cooldown layer keyed by stable job/message cause, NPC/job/site where appropriate.
3. Preserve first occurrence and meaningful state changes.
4. Add explicit typed exceptions for locker-zone and patrol-zone work:
   - locker-zone messages should still surface real gear/readiness failures once with enough reason to debug;
   - patrol-zone messages should still surface actual guard assignment, broken patrol, danger interruption, or reserve/backfill state changes;
   - routine repeats from those systems should be compressed rather than hidden or spammed.
5. Keep the behavior local to Basecamp/camp job reporting; do not suppress unrelated combat, player action, or dialogue messages.

## Non-goals

- No global message-log rewrite.
- No hiding real errors just to make the log pretty.
- No changing patrol roster design or locker equipment policy in this packet.
- No Smart Zone Manager layout changes.
- No bandit AI changes.

## Success state

- [x] Repeated Basecamp completion/missing-tool/no-progress messages are debounced by stable cause so they do not flood the visible log.
- [x] First occurrence and changed state still produce a visible/reportable message.
- [x] Locker-zone work has a typed exception path: real locker gear/readiness failures remain visible once with reason, while repeats are compressed.
- [x] Patrol-zone work has a typed exception path: real assignment/reserve state changes remain visible once with reason, while repeats are compressed.
- [x] The debounce state does not survive in a way that hides messages forever after save/load or unrelated job changes.
- [x] Deterministic tests cover ordinary repeated spam, changed-state reset, locker exception, patrol exception, and a negative case showing unrelated important messages are not swallowed.
- [x] If practical, a harness/log proof shows the old spam shape is reduced without losing one meaningful locker/patrol message. Local deterministic/log-message gates were sufficient; no broader harness packet was needed for this narrow message-debounce slice.

## Suggested validation packet

- Deterministic camp job reporting tests for:
  - repeated same completion -> compressed;
  - repeated same missing-tool cause -> compressed;
  - changed missing tool/job/NPC -> visible again;
  - locker repeated failure -> one typed visible reason, then compression;
  - patrol repeated routine state -> compression, but true interruption/backfill remains visible.
- Narrow code gates:
  - touched-object compile for changed camp/job/message sources;
  - focused camp/patrol/locker tests;
  - `git diff --check`.

## Handoff note for Andi

Do not just mute the waiter because the kitchen is noisy. Keep the first useful message and state changes; compress the echo. Locker and patrol get named exception handling because those are the places Josef actually needs to debug when the camp acts weird.