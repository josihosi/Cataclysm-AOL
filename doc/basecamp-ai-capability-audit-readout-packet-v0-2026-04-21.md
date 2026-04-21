# Basecamp AI capability audit/readout packet v0 (2026-04-21)

Status: greenlit camp-side audit packet.

## Greenlight verdict

Josef explicitly wants a clear read on what Basecamp AI can already do, because the original "craft via a sentence" goal has accreted enough extra board/action plumbing that the current capability surface is no longer obvious.

The next honest step is not prompt externalization yet.
The next honest step is one bounded capability audit/readout packet that maps the current Basecamp AI surface cleanly enough that later feature, cleanup, or prompt decisions stop happening in fog.

## Scope

1. Inventory the current Basecamp AI capability surface on the current tree.
2. Separate at least these layers reviewer-cleanly:
   - player-facing spoken command surface
   - internal structured action/token surface
   - board/job inspection and control surface
   - any still-prompt-shaped or otherwise unclear interpretation layer, if it exists
3. Produce one reviewer-readable capability packet that says plainly what the player can ask for, what internal actions/tokens exist, and what remains unclear or only partially implemented.
4. Ground the packet in real current code/tests/evidence instead of only old planning prose.

## Non-goals

- prompt externalization itself
- fresh Basecamp AI architecture or feature expansion
- quietly replacing `Locker Zone V3` as the camp-side implementation follow-through
- turning the packet into a new general technical tome for every camp mechanic
- rewriting the active bandit queue

## Success shape

This item is good enough when:
1. one bounded capability audit/readout packet exists for the current Basecamp AI surface
2. the packet distinguishes player-facing spoken behaviors from internal structured actions/tokens instead of mushing them together
3. the packet says plainly what board/job actions are actually supported now
4. the packet says plainly whether any prompt-shaped interpretation layer still matters, or whether the current behavior is mostly deterministic plumbing already
5. the packet is grounded in current code/tests/evidence strongly enough that later cleanup decisions do not rely on stale folklore
6. the slice stays bounded and does not mutate into implementation work by accident

## Validation expectations

- use current code, tests, existing artifacts, and targeted repo inspection first
- if a small probe is needed to settle ambiguity, keep it proportional and explicit
- do not call old planning intent evidence of current capability by itself
- if the audit finds contradictions between canon and code, surface them plainly instead of smoothing them over

## Position in canon

This is a greenlit camp-side audit packet.
It does not replace the repo's current single active `TODO.md` lane.
It exists so later camp-side decisions, especially any prompt-externalization talk, are based on a real map instead of vibes.
