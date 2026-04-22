# Bandit + Basecamp playtest kit packet v2 (2026-04-22)

Status: FOLDED INTO `bandit-live-world-control-playtest-restage-packet-v0-2026-04-22` / SUPPORTING HELPER WORK ONLY.

## Greenlight verdict

Josef explicitly wanted the playtest harness to become broad enough that Schani and Andi can meaningfully probe every important nook and cranny of the plan, not merely reload a few static saves.
That helper/tooling need still stands.

But the later product correction was sharper: helper breadth alone is not the product if the live bandit system still does not honestly own and control the real spawned bandits.
So this contract now survives only as supporting helper/tooling scope underneath `Bandit live-world control + playtest restage packet v0`, rather than as a standalone headline lane.

## Why this packet exists

The current harness already uses a little of the debug surface, but much of that power is still lying around unused like expensive cutlery in a drawer.
If the goal is truly broad playtestability, the harness needs to mutate scenarios on purpose instead of only loading prepared fixtures and hoping they happen to fit.

## Scope

1. Expand the harness/debug setup surface so common playtest mutations become first-class repeatable helpers instead of keyboard folklore.
2. Use more of the existing debug-menu capability where practical before inventing deeper infrastructure.
3. Add named scenario-surgery helpers for important setup dimensions such as:
   - mutations / traits
   - NPC gear / loadouts
   - faction or hostility posture
   - wounds / healing / status
   - weather / time / light
   - food / stockpile fill
   - camp role / stationing / patrol readiness
4. Add reusable transforms/presets that can start from a good prepared fixture and apply named deltas such as quiet-management morning, night watch, bad weather, hungry camp, armed patrol, hostile contact nearby, follower convoy, and similar bounded variants.
5. Improve observability so the harness can say what it changed and what the operator should now expect to see, instead of only claiming that a setup step ran.
6. Prefer practical first-class harness verbs over endless manual debug-menu choreography when the same scenario mutation keeps recurring.

## Non-goals

- a total harness rewrite in one go
- replacing every useful captured fixture with pure transforms
- speculative mechanics work disguised as harness work
- an infinite general-purpose game-modding platform
- magical certainty that every world state can be authored perfectly on day one

## Success shape

This packet is good enough when:
1. the harness can apply a meaningful set of named scenario mutations beyond the tiny current helper subset
2. important near-term playtest variants can be produced from prepared footing without duplicating whole saves for every minor difference
3. the operator can tell what was changed, what state was forced, and what on-screen consequences should now be expected
4. Schani and Andi can set up a materially broader slice of the Cataclysm-AOL plan surface without rebuilding everything by hand
5. recurring manual debug rituals shrink because the important ones became named reusable helpers
6. the packet stays focused on scenario surgery and observability rather than turning into abstract infrastructure vanity

## Validation expectations

This packet will likely mix harness code, setup helpers, and live proof.
Validation should therefore stay explicit about evidence class:

- deterministic checks or narrow harness tests for helper logic that can be tested without the full game
- fresh live proof for representative high-value scenario mutations
- reviewer-readable artifact output that shows both the forced setup and the resulting live state
- honest separation between what is directly driven, what still depends on captured fixtures, and what remains manual

## Review questions this packet should answer

- Can the harness now mutate scenarios broadly enough that the important planned seams become genuinely playtestable by Schani and Andi?
- Are we exploiting the practical debug surface well before asking for deeper engine surgery?
- Did the new helpers reduce duplicated save sprawl and manual setup ritual?
- Is the observability good enough that setup failures stop disappearing into folklore?

## Skill sync requirement

Any named scenario mutations, transforms, debug-surface helpers, or observability/reporting conventions that land from this packet should also be folded back into the C-AOL harness skill so later agents can discover them without spelunking old packet docs.
For the broader generic skill package on this surface, Andi should draft the additions and Frau Knackal should review the skill before it is treated as settled.

## Position in the sequence

This packet sits behind playtest kit v1.
V1 provides rich prepared-base footing.
V2 then turns that footing into a broader scenario-surgery surface with reusable transforms and stronger observability.
