# Basecamp job spam debounce + locker/patrol exceptions packet v0 (2026-04-26)

Status: active / greenlit.

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

- [ ] Repeated Basecamp completion/missing-tool/no-progress messages are debounced by stable cause so they do not flood the visible log.
- [ ] First occurrence and changed state still produce a visible/reportable message.
- [ ] Locker-zone work has a typed exception path: real locker gear/readiness failures remain visible once with reason, while repeats are compressed.
- [ ] Patrol-zone work has a typed exception path: real assignment/interruption/reserve/backfill changes remain visible once with reason, while repeats are compressed.
- [ ] The debounce state does not survive in a way that hides messages forever after save/load or unrelated job changes.
- [ ] Deterministic tests cover ordinary repeated spam, changed-state reset, locker exception, patrol exception, and a negative case showing unrelated important messages are not swallowed.
- [ ] If practical, a harness/log proof shows the old spam shape is reduced without losing one meaningful locker/patrol message.

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