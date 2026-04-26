# Basecamp locker armor ranking + blocker removal packet v0 (2026-04-26)

Status: greenlit / queued.

## Normalized contract

**Title:** Basecamp locker armor ranking + blocker removal packet v0

**Request kind:** Josef greenlight / queued implementation package

**Summary:** Josef confirmed the visible `RM13 combat armor` / jeans spam is not an RM13-specific bug. The real problem is generic: camp locker automation needs a reviewer-readable metric for deciding when a candidate full-body or high-coverage protective item is better than currently worn blockers, then it should remove/drop only the blocking pieces needed for that better item instead of repeatedly failing and making NPCs visibly re-wear junk.

## Scope

1. Replace any RM13-specific thinking with a generic armor comparison policy.
2. Define one small scoring/decision helper for candidate protective gear versus currently worn blockers.
   - Weight body parts by gameplay importance, with chest/torso highest, then legs, then head, then hands/arms/feet as appropriate for the actual slot model.
   - Consider protection, coverage, encumbrance cost, condition/damage, and the active locker policy toggles such as ballistic preference.
   - Keep the score explainable; no giant black-box fashion oracle.
3. If the candidate is clearly better for the active policy, remove/drop the blocking worn items required to equip it.
4. If the candidate is not clearly better, stop retrying the failed equip path and do not spam visible wear messages.
5. Preserve already-proven direct-current behavior where stronger existing ballistic armor should not be displaced by worse full-body gear.
6. Extend or reuse existing locker combat-policy tests instead of building a parallel RM13 shrine.

## Non-goals

- No RM13 hardcode.
- No broad NPC outfit/fashion rewrite.
- No tactical loadout AI for every possible weather/social/combat context.
- No inventory clairvoyance outside the current locker/service path.
- No reopening the whole Locker Zone V3 lane unless this narrow scoring helper proves the old success state was false.
- No visible player-message suppression that hides genuinely important manual equipment actions.

## Success state

- [ ] A generic helper scores candidate protective/full-body gear against currently worn blockers using body-part priority, protection/coverage, encumbrance, condition, and active locker policy.
- [ ] The helper is not item-id-specific and does not special-case `RM13 combat armor`.
- [ ] When a candidate is clearly superior, the locker path removes/drops the blocking worn items needed to equip it.
- [ ] When a candidate is not clearly superior or cannot be equipped, the locker path stops retrying the same failed swap and does not produce visible repeated wear spam.
- [ ] Existing superior-full-body and ballistic-maintenance tests are reused/extended, including positive and negative cases.
- [ ] Tests prove a clearly superior full-body/protective suit can displace worse blockers, while stronger current ballistic armor is preserved against worse candidates.
- [ ] At least one targeted regression covers the original symptom shape without depending on the exact RM13 item ID as the only proof.

## Suggested validation packet

- Focused deterministic tests in the locker/combat policy family:
  - superior full-body candidate displaces worse shirt/pants/vest blockers;
  - worse full-body candidate does not displace stronger existing armor;
  - damaged/encumbering candidate loses when appropriate;
  - same failed candidate does not keep requeueing visible equip spam.
- Narrow build/test:
  - touched-object compile for `src/basecamp.cpp` or any split helper;
  - focused `./tests/cata_test "[camp][locker]"` / relevant combat-policy filter;
  - `git diff --check`.
- If a harness proof is cheap, add one visible-log control showing the old repeated wear-spam shape is gone.

## Handoff note for Andi

Start from the existing armor/locker tests. Do not create an RM13 exception. The wanted behavior is “better armor wins and blockers get cleared,” not “the cursed spacesuit gets a royal decree.”