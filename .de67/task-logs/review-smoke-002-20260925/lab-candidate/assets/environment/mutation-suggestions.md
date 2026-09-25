# DE-67 mutation suggestion ledger

This is a consumable mutation queue, not history. User-authored entries carry the explicit
mutation-scoped owner authority defined by the Phase-3 guidance; reviewer-authored candidates remain
evidence-backed proposals. A successful review removes only the entries it completed. Preserve any
blocked entry in place with its exact system/developer conflict, technical impossibility, unavailable
external authority, or irreversible risk. A failed review removes nothing. Ledger cleanup never
blocks unrelated ordinary work.

Prefix every new user entry with one of these modes:

- `Owner-authorized [trigger]:` requests an exclusive mutation review as soon as workers are quiet.
- `Owner-authorized [defer]:` stays queued without waking or retiring the coordinator. The next
  deadline, integrity, random, or otherwise regularly due mutation review consumes it as mandatory
  input.

An older unlabelled entry keeps the legacy `trigger` behavior. Changing modes does not change the
entry's mutation-scoped owner authority. A reviewer removes an entry only after applying or exactly
dispositioning it; supervisor recovery cannot consume it a second time.

Every deadline miss records both:

- a micro recovery for the same item: contradicted premise, changed strategy or observation route,
  and next frozen closure gaps;
- a macro method candidate: the role, guideline, FS, task/test/debug, or orchestration change that
  the evidence supports.

Mark candidate scope and whether it is guarded, isolated, rejected, or awaiting owner authority.
Never imply that a narrow guard validated a broader candidate.

A random review follows a recent trajectory from assigned outcome through available context,
decisions, actions, and actual proof or state change. Its stored target is a sampling seed, not an
edit boundary. Record useful findings with direct evidence and the smallest supported correction;
there is no finding quota or obligation to mutate. Preserve legitimate repetition after changed
inputs and independent validation. A guarded no-op resolves a review with no justified improvement.
Broader method candidates use their existing validation boundary. Consume only suggestions actually
resolved by the review.

A rare universal review exists only for a persisted `k = 30` plus historical `DFS.md` receipt lane with proved
`gpt-6-sol`/`ultra` capability. It records the isolated whole-method or FS candidate and
falsifiable proof plan. The candidate may challenge kernel, clock, guard, and supervisor
architecture, but it cannot edit live state, self-promote, weaken the user outcome or honest proof,
falsify evidence, exceed authority, or erase attempt accounting.

## Pending suggestions

None.
