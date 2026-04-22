# Plan

Canonical roadmap for Cataclysm-AOL.

This file answers one question: **what should the agent advance next?**
It is not a changelog, not a graveyard of crossed-off substeps, and not a place to preserve every historical detour.
Rewrite it as reality changes.

## File roles

- **Plan.md** - canonical roadmap and current delivery target
- **SUCCESS.md** - success-state ledger / crossed-off exit criteria for roadmap items
- **TODO.md** - short execution queue for the current target only
- **TESTING.md** - current validation policy, latest relevant evidence, and pending probes
- **TechnicalTome.md** - durable mechanic notes, not daily state tracking
- **COMMIT_POLICY.md** - checkpoint rules to prevent repo soup

If these files disagree, **Plan.md wins** and the other files should be repaired.

## Working rules for agents

- Do **not** mechanically grab the first unchecked-looking thing from some list.
- Follow the current delivery target below and move it to its **next real state**.
- Josef being unavailable for playtesting is **not** a blocker by itself.
- When a target is waiting on Josef, move to the next best unblocked target.
- If no good unblocked target remains, send Josef a short parked-options note so he can greenlight the next lane; do not just keep revalidating the old packet.
- Prefer batching human-only asks where practical. One useful packet with two real product questions beats two tiny pings.
- Keep these files lean. Remove finished fluff from `TODO.md` and `TESTING.md` instead of piling up crossed-off archaeology.
- Each real roadmap item needs an explicit success state in `SUCCESS.md` (or an equally explicit inline auxiliary) so completion is visible instead of guessed.
- Cross off reached success-state items; only remove the whole roadmap item from `Plan.md` once its success state is fully crossed off / done.
- Prefer agent-side playtesting first. Josef should be used for product judgment, feel, priority calls, or genuinely human-only checks.
- Validation should match risk:
  - docs-only change -> no compile
  - small local code change -> narrow compile/test
  - broad or risky code change, or a Josef handoff -> broader rebuild / startup harness as needed
- Follow `COMMIT_POLICY.md`. Do not let the repo turn into one giant dirty blob.

---

## Current status

The active greenlit repo lane is now **Bandit + Basecamp first-pass encounter/readability packet v0**.

Current honest state:
- the authoritative active contract now lives at `doc/bandit-basecamp-first-pass-encounter-readability-packet-v0-2026-04-22.md`
- the latest closed lane is `Live bandit + Basecamp playtest packaging/helper packet v0` at `doc/live-bandit-basecamp-playtest-packaging-helper-packet-v0-2026-04-22.md`
- the landed helper is `tools/openclaw_harness/scenarios/bandit.basecamp_named_spawn_mcw.json`, packaging the already-proved named-bandit restage seam on reused McWilliams/Basecamp footing
- fresh current-build helper proof under `.userdata/dev-harness/harness_runs/20260422_132353/` now means the next live pass can judge encounter readability instead of arguing about setup existence
- the active job now is one bounded first-pass live encounter/readability packet on top of that helper path, focused on what the player can actually read and understand on screen when a hostile bandit is intentionally restaged on real Basecamp footing
- Josef explicitly greenlit one larger cohesive queued-next follow-through, `Bandit + Basecamp playtest kit packet v0`, to harden the playtest surface before his near-term manual playtest window without widening into a custom-map framework or fresh mechanics spree
- Josef then widened that runway further into a deliberate playtest-kit stack: `v1` as a rich prepared-base fixture family and `v2` as a debug-power scenario-surgery layer, both ordered behind `v0` instead of left as chat vapor
- `Locker Zone V3` stays checkpointed closed at `doc/locker-zone-v3-reopen-packet-v0-2026-04-21.md`; this live packet should reuse current camp footing, not reopen zoning mechanics by drift
- the current bandit playtesting-readiness train stays checkpointed closed for now, including the handoff interaction packet, elevated-light / z-level visibility packet, benchmark suite, weather refinement, pressure rewrite, long-range directional light, bounded scout/explore, scoring refinement, moving-bounty memory, and repeated-site revisit follow-through
- `Bandit z-level visibility proof packet v0` does not come back as a vague side branch; the bounded home for that work was `doc/bandit-elevated-light-and-z-level-visibility-packet-v0-2026-04-21.md`, and that packet stays closed unless new evidence says it lied

This lane is about first-pass live readability/product truth, not fresh mechanics or broad tuning.

---

## Active lane - Bandit + Basecamp first-pass encounter/readability packet v0

**Status:** ACTIVE / GREENLIT

Use the packaged helper path `bandit.basecamp_named_spawn_mcw` for the first real live encounter/readability pass on current Basecamp footing, and capture reviewer-readable artifacts strong enough to judge whether the encounter is legible, alive, and worth further follow-through.

Canonical contract lives at `doc/bandit-basecamp-first-pass-encounter-readability-packet-v0-2026-04-22.md`.

---

## Greenlit next - Bandit + Basecamp playtest kit packet v0

**Status:** GREENLIT / QUEUED NEXT

Package one cohesive pre-playtest kit on current bandit + Basecamp footing: harden `bandit.basecamp_named_spawn_mcw` for repeatable operator use, improve screen-first artifact readability, clean up leftover live-window/session clutter, and add one purpose-built fast-reload playtest fixture/save pack with a small named scenario family.

Canonical contract lives at `doc/bandit-basecamp-playtest-kit-packet-v0-2026-04-22.md`.

---

## Greenlit after v0 - Bandit + Basecamp playtest kit packet v1

**Status:** GREENLIT / QUEUED AFTER V0

Build a small rich prepared-base fixture family for current Basecamp-management and bandit-interplay probing: management-ready camp footing, relevant zones and supplies, armed NPCs, patrol/locker/smart-zone support, and a player fixture carrying both clairvoyance mutations for cleaner live debugging/readability.

Canonical contract lives at `doc/bandit-basecamp-playtest-kit-packet-v1-2026-04-22.md`.

---

## Greenlit after v1 - Bandit + Basecamp playtest kit packet v2

**Status:** GREENLIT / QUEUED AFTER V1

Turn the harness into a broader scenario-surgery surface on top of prepared fixtures: use more of the practical debug power, add named mutations/transforms for common setup states, and make the operator-visible change report strong enough that Schani and Andi can probe much more of the plan without hand-rebuilding worlds.

Canonical contract lives at `doc/bandit-basecamp-playtest-kit-packet-v2-2026-04-22.md`.

---

## Checkpointed packet index

Use the auxiliary docs below when a later discussion needs the canonical contract or the honest closed verdict, not as permission to reopen the lane automatically.

### Camp / Basecamp packets

- `doc/bandit-basecamp-first-pass-encounter-readability-packet-v0-2026-04-22.md` (active)
- `doc/bandit-basecamp-playtest-kit-packet-v0-2026-04-22.md` (greenlit / queued next)
- `doc/bandit-basecamp-playtest-kit-packet-v1-2026-04-22.md` (greenlit / queued after v0)
- `doc/bandit-basecamp-playtest-kit-packet-v2-2026-04-22.md` (greenlit / queued after v1)
- `doc/live-bandit-basecamp-playtest-packaging-helper-packet-v0-2026-04-22.md`
- `doc/locker-zone-v3-reopen-packet-v0-2026-04-21.md`
- `doc/basecamp-ai-capability-audit-readout-packet-v0-2026-04-21.md`
- `doc/live-bandit-basecamp-playtesting-feasibility-probe-v0-2026-04-21.md`

### Bandit readiness and follow-through packets

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

### Earlier bandit substrate docs still worth keeping straight

- `doc/bandit-concept-formalization-followthrough-2026-04-19.md`
- `doc/bandit-player-basecamp-visibility-and-concealment-2026-04-19.md`
- `doc/bandit-overmap-ai-concept-2026-04-19.md`

---

## Parked concept chain - Bandit overmap AI

**Status:** PARKED / COHERENT SUBSTRATE

This larger concept stays parked as a planning substrate.
Several bounded `v0` slices were promoted, implemented, and checkpointed, but that does **not** silently greenlight the remaining broader concept work.

Current parked-chain anchor:
- `doc/bandit-overmap-ai-concept-2026-04-19.md`

Still-parked concept docs:
- `doc/bandit-mark-generation-and-heatmap-model-2026-04-19.md`
- `doc/bandit-overmap-to-bubble-handoff-seam-2026-04-19.md`
- `doc/bandit-player-basecamp-visibility-and-concealment-2026-04-19.md`

Promoted slices that are now checkpointed closed live in the packet index above.
If this chain is revisited later, the next discussion should be about one new bounded promotion or one real contradiction in current canon, not about spawning another fog bank of feeder docs.

---

## Future feature lanes - parked far back

These lanes are **not part of the current camp-handling or bandit queue**.
Keep them visibly separate so adjacent tooling or observability work does not get mistaken for partial feature delivery.

- Chat interface over in-game dialogue branches
- Tiny ambient-trigger NPC model
- Arsenic-and-Old-Lace social threat and agentic-world concept bank
  - anchor doc: `doc/arsenic-old-lace-social-threat-and-agentic-world-concept-bank-2026-04-22.md`
  - preserve this as a far-back parked concept bank, not a disguised implementation queue
  - the strongest near-promotable seeds currently include alarm states via natural-language yelling, bandits exploiting readable camp routines, radio information warfare, writhing-stalker pressure, and social camouflage / hidden-psychopath survivor play
  - broader far-out families worth keeping include agentic NPC goals, interdimensional-traveler motives, conspiracy pressure, and other weird-world social-horror systems
  - do **not** let this bank outrun honest playtesting of the current bandit and Basecamp zoning footing; unpack one bounded item at a time only after the present substrate is reviewer-clean enough to deserve more load

Do not reopen them by drift.
They stay buried until Josef explicitly promotes them.

---

## Documentation discipline

If the structure starts bloating again, apply this rule:
- `Plan.md` should be readable in a minute
- `TODO.md` should show only the current execution queue
- `TESTING.md` should show only current policy, latest relevant evidence, and pending probes
- `COMMIT_POLICY.md` should stop the dirty tree from becoming a lifestyle

If a sentence exists only to remember that something used to be true, it probably belongs in git history, not here.
