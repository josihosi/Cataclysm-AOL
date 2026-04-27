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
- During the current debug-proof finish stack, failed agent-side proof does **not** close or park implemented code. After the attempt budget, move implemented-but-unproven items to Josef's playtest package and continue the next greenlit debug note.
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

Repo policy remains unchanged: `/Users/josefhorvath/Schanigarten/Cataclysm-AOL` on `dev` is the normal worktree and `josihosi/Cataclysm-AOL` is the real project/release repo. `josihosi/C-AOL-mirror` is green-dot-only.

Detailed contracts, closure evidence, and older checkpoint history belong in `doc/*.md`, `SUCCESS.md`, and git history. Keep this file short enough that the active stack is visible without archaeology.

---

## Closed lane - Basecamp locker armor ranking + blocker removal packet v0

**Status:** CLOSED / CHECKPOINTED

Generic protective/full-body armor ranking and blocker clearing are landed for the camp locker path without special-casing `RM13 combat armor`. Deterministic proof covers superior full-body candidates displacing worse blockers, damaged candidates being rejected without repeated requeue/equip churn, stronger ballistic armor being preserved while a compatible full-body suit is added, and `[camp][locker]` readiness regression coverage.

Canonical contract lives at `doc/basecamp-locker-armor-ranking-blocker-removal-packet-v0-2026-04-26.md`.

---

## Closed lane - Basecamp job spam debounce + locker/patrol exceptions packet v0

**Status:** CLOSED / CHECKPOINTED

Stable-cause camp job chatter debounce landed for camp activity completion, camp request blocked/missing-tool barks, and no-progress request barks. Locker and patrol paths now use typed `[camp][locker]` / `[camp][patrol]` reports with repeated-state compression, preserving first occurrence and changed state visibility without a global message-log rewrite.

Canonical contract lives at `doc/basecamp-job-spam-debounce-exceptions-packet-v0-2026-04-26.md`.

---

## Completed lane - C-AOL debug-proof finish stack v0

**Status:** CLOSED / CHECKPOINTED

Josef explicitly reopened the current C-AOL debug-proof notes on 2026-04-27. The stack has now reached an honest agent-side boundary without leaving failed proof in parked/review-only posture.

- Bandit live signal/site-bootstrap product-proof work reached its honest agent-side boundary: synthetic loaded-map fire/light reaches the running live path and the real player-lit fire bridge is listed in Josef's playtest package as implemented-but-unproven; do not loop on it again without a fresh reopen/material blocker change.
- Smart Zone Manager live layout separation correction reached its honest agent-side boundary: the planner now keeps intended-separate generated zones on separate reserved tiles with deterministic geometry/separation coverage, and the failed clean live/UI macro has been packaged for Josef as implemented-but-unproven instead of using the contaminated old McWilliams macro as closure proof.
- Bandit local standoff / scout return live correction is now fixed/proven on the current live product path: deterministic coverage asserts the pre-local-contact scout timeout, rebuilt current-runtime run `.userdata/dev-harness/harness_runs/20260427_154309/` used the real `wait_action` path, logged `local_gate ... posture=hold_off ... standoff_distance=5 ... live_dispatch_goal=140,46,0`, then logged `scout_sortie: linger limit reached -> return_home`, `scout_sortie: home footprint observed ... pos=(140,51,0)`, and `scout_report: returned -> pressure refreshed`.
- Attempt rule for this stack was observed: attempt 3 happened only after Frau Knackal consultation and a material code-path/instrumentation change.
- **GitHub normal-download release packet v0** should stay held until Schani/Josef decide whether the greenlit harness trust audit/proof-freeze package or release posture is next.

Canonical contract lives at `doc/caol-debug-proof-finish-stack-v0-2026-04-27.md`.

---

## Active lane - C-AOL harness trust audit + proof-freeze v0

**Status:** ACTIVE / GREENLIT PROCESS AUDIT

Josef requested this on 2026-04-27 after watching harness runs load the game, close it, and still risk being treated as feature proof. This is a large audit of the C-AOL harness skill/procedures, preferably Andi execution with Frau Knackal review: inventory every harness primitive, prove every keystroke/setup step with screenshots or exact game metadata, enforce same-save/provenance discipline where possible, and freeze the workflow so false passes become red states instead of polite nonsense.

This package does not replace the current active Smart Zone / bandit debug-proof stack unless Josef explicitly promotes it, but it does set the trust rule for future closure claims: load-and-close is startup/load proof only, and harness-based feature proof must show step-by-step evidence for the actual feature path.

Canonical contract lives at `doc/c-aol-harness-trust-audit-and-proof-freeze-packet-v0-2026-04-27.md`.

Current checkpoint: harness surface inventory and provisional same-save policy live at `doc/c-aol-harness-trust-audit-inventory-v0-2026-04-27.md`. This is an inventory/provenance boundary only, not a feature-proof freeze; the next honest state is step-ledger audit of launch/load/focus/screenshot on the canonical `dev-harness` / `McWilliams` anchor.

---

## Recently closed correction checkpoints

**Status:** CHECKPOINTED / CLOSED

Do not reopen these by drift:
- **Bandit live-wiring audit + visible-light horde bridge correction v0** — loaded-map visible fire/light -> horde signal bridge proof, not player-lit fire proof.
- **Bandit local sight-avoid + scout return cadence packet v0** — reclosed for the 2026-04-27 product gap: current-runtime live proof now covers five-OMT hold-off standoff plus scout timeout, home-footprint observation, and returned pressure-refresh writeback on the McWilliams/Basecamp product path.
- **Smart Zone Manager v1 Josef playtest corrections** — implemented-but-unproven live: deterministic geometry/separation proof is green and the clean live/UI macro failure is in Josef's playtest package; do not loop on it again without a fresh reopen or materially repaired harness primitive.
- **Basecamp medical consumable readiness v0** — deterministic camp/locker proof for bounded bandage-family readiness, including carried-stock cap behavior.
- **Basecamp locker armor ranking + blocker removal packet v0** — generic protective/full-body armor comparison and blocker clearing proof; no RM13 special case.

---

## Checkpointed packet index

**Status:** CHECKPOINTED / INDEX

Use the auxiliary docs below when a later discussion needs the canonical contract or the honest closed verdict, not as permission to reopen the lane automatically.

### Camp / Basecamp packets

- `doc/bandit-extortion-playthrough-audit-harness-skill-packet-v0-2026-04-22.md` (checkpointed / done for now)
- `doc/bandit-extortion-at-camp-restage-handoff-packet-v0-2026-04-22.md` (checkpointed / done for now)
- `doc/bandit-aftermath-renegotiation-writeback-packet-v0-2026-04-22.md` (checkpointed / done for now)
- `doc/bandit-shakedown-pay-or-fight-surface-packet-v0-2026-04-22.md` (checkpointed / done for now)
- `doc/bandit-approach-stand-off-attack-gate-packet-v0-2026-04-22.md` (checkpointed / done for now)
- `doc/bandit-live-world-control-playtest-restage-packet-v0-2026-04-22.md` (checkpointed / done for now)
- `doc/bandit-basecamp-playtest-kit-packet-v1-2026-04-22.md` (checkpointed / done for now)
- `doc/bandit-basecamp-playtest-kit-packet-v2-2026-04-22.md` (folded into later active lane / supporting only)
- `doc/bandit-basecamp-playtest-kit-packet-v0-2026-04-22.md`
- `doc/bandit-basecamp-first-pass-encounter-readability-packet-v0-2026-04-22.md`
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

**Status:** PARKED / FAR BACK

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
