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
- **doc/work-ledger.md** - compact receipt book for asks, state transitions, evidence links, and owners
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

`doc/work-ledger.md` is now the compact receipt book for meaningful asks, state changes, evidence links, owners, supersessions, held lanes, and red/non-credit proof. Use it before trimming active docs.

Detailed contracts, closure evidence, and older checkpoint history belong in `doc/*.md`, `doc/work-ledger.md`, `SUCCESS.md`, and git history. Keep this file short enough that the active stack is visible without archaeology.

---

## Active lane — CAOL-MULTI-CAMP-SIGNAL-GAUNTLET-v0

**Status:** ACTIVE / GREENLIT CHALLENGE PLAYTEST PACKET

Josef greenlit a real Andi challenge on 2026-04-30: “make it good” and give Andi “a real challenge or a couple challenges”. This active packet combines the strongest current follow-ups into one bounded gauntlet: multi-camp structural stress, mixed signal coexistence, and reload/resume continuity if practical.

Contract: `doc/multi-camp-signal-gauntlet-playtest-packet-v0-2026-04-30.md`.

Imagination source: `doc/multi-camp-signal-gauntlet-imagination-source-of-truth-2026-04-30.md`.

Goal: prove that live bandit overmap systems stay sane when multiple camps, structural bounty, and live smoke/fire/light-style signals coexist under bounded time passage. The gauntlet should report target choice, dogpile/spread/hold behavior, active outing state, save/reload continuity if available, and cost/stability metrics.

Boundary: do **not** reopen roof-horde, writhing-stalker, Smart Zone, or old fire proof lanes. Do **not** call setup-only proof a challenge result. Staged-but-live scenarios are acceptable, but live maintenance/outing paths must run and produce before/after evidence.

Last closed active lane: `CAOL-ROOF-HORDE-NICE-FIRE-v0` — see `doc/roof-fire-horde-nice-roof-fire-proof-v0-2026-04-30.md`, `doc/work-ledger.md`, `SUCCESS.md`, and `TESTING.md`.

---

## Greenlit queued lane — CAOL-WRITHING-STALKER-PATTERN-TESTS-v0

**Status:** GREENLIT / QUEUED AFTER ACTIVE GAUNTLET

Josef greenlit a writhing-stalker behavior-pattern follow-up on 2026-04-30 after clarifying the desired attack rhythm: the stalker should be able to attack a few times when healthy and the opportunity stays good, then withdraw once badly injured. This is queued behind `CAOL-MULTI-CAMP-SIGNAL-GAUNTLET-v0`; do not interrupt the active gauntlet unless Schani/Josef explicitly promote this packet to active.

Contract: `doc/writhing-stalker-behavior-pattern-minimap-packet-v0-2026-04-30.md`.

Imagination source: `doc/writhing-stalker-behavior-pattern-imagination-source-of-truth-2026-04-30.md`.

Goal: add a tiny primitive minimap/ASCII/equivalent repeated-turn behavior-pattern test layer for writhing-stalker stalking cadence: no-evidence/no-beeline, weak evidence decay, cover/edge route preference, exposure hold/withdraw, vulnerability strike windows, cooldown anti-spam, repeated attack cadence, badly-injured retreat, and jitter/stuckness smells. If current code already passes, record the proof; if not, make the smallest tuning/implementation change that preserves no-omniscience and anti-spam constraints.

Boundary: this is not a monster-flavor/stat respin, not a mixed-hostile rerun, and not a full long-term nemesis-memory design. Do not make the stalker nastier by making it omniscient.

---

## Held / parked lanes that must not disappear

**Status:** PARKED / HELD VISIBILITY ONLY

These are visible here only because they affect future selection. Do not treat them as active without explicit Schani/Josef promotion.

- `CAOL-GITHUB-RELEASE-v0` — **HELD / NOT PROMOTED**. Normal GitHub-download release work remains held until Schani/Josef explicitly promote it. Contract: `doc/github-normal-download-release-packet-v0-2026-04-25.md`.
- `CAOL-BANDIT-OVERMAP-AI-CONCEPT-v0` — **PARKED / COHERENT SUBSTRATE**. Parent concept: `doc/bandit-overmap-ai-concept-2026-04-19.md`; bounded promoted slices need their own IDs/contracts.
- `CAOL-AOL-SOCIAL-THREAT-BANK-v0` — **PARKED / FAR BACK**. Anchor: `doc/arsenic-old-lace-social-threat-and-agentic-world-concept-bank-2026-04-22.md`. Writhing stalker has now been promoted out of this bank into its own active packet; the rest remains parked.

---

## Recent closed receipts

Use `doc/work-ledger.md` and the linked aux docs for exact evidence, caveats, and supersessions. Do not reopen these by drift.

- `CAOL-ROOF-HORDE-NICE-FIRE-v0` — closed/checkpointed green v0; focused nice roof-fire horde playtest using source player-created roof-fire chain `.userdata/dev-harness/harness_runs/20260429_172847/`, named scenario `bandit.roof_fire_horde_nice_roof_fire_mcw`, and green run `.userdata/dev-harness/harness_runs/20260430_191556/`. Proof reaches live roof-fire horde signal plus saved horde retarget/destination, `last_processed=5267242`, and `moves=8400`; caveats: `tracking_intensity=0` and horde-specific timing `not instrumented`. Proof doc `doc/roof-fire-horde-nice-roof-fire-proof-v0-2026-04-30.md`.
- `CAOL-WRITHING-STALKER-v0` — closed/checkpointed green v0; first playable rare singleton zombie-adjacent predator with deterministic interest/latch/approach/opportunity/withdraw substrate, live monster-plan seam, exposed-retreat proof, shadow/strike/cooldown proof, no-omniscience negative control, and mixed-hostile metrics/tuning readout. Caveat preserved: horde presence/setup is proven in `.userdata/dev-harness/harness_runs/20260430_181748/`, but direct horde movement/retarget cost is not instrumented and is future-only unless explicitly promoted.
- `CAOL-BANDIT-STRUCT-BOUNTY-v0` — closed/checkpointed green v0; closure readout `doc/bandit-structural-bounty-phase-7-closure-readout-2026-04-30.md`; green run `.userdata/dev-harness/harness_runs/20260430_115157/`; non-credit run `.userdata/dev-harness/harness_runs/20260430_114106/`.
- `CAOL-SZM-LIVE-LABEL-v0` — closed green live coordinate-label proof; run `.userdata/smart-zone-audit-live-20260429e/harness_runs/20260429_225644/`.
- `CAOL-CANNIBAL-NIGHT-RAID-v0` — closed cannibal camp night-raid behavior checkpoint; contract `doc/cannibal-camp-night-raid-behavior-packet-v0-2026-04-28.md`.
- `CAOL-CANNIBAL-CONFIDENCE-PUSH-v0` — closed confidence-uplift proof; matrix `doc/cannibal-camp-confidence-push-live-playtest-matrix-v0-2026-04-29.md`.
- `CAOL-BANDIT-CAMP-MAP-RISK-REWARD-v0` — closed scoped live/product checkpoint; matrix `doc/bandit-live-product-playtest-matrix-v0-2026-04-28.md`.
- `CAOL-HARNESS-PROOF-FREEZE-v0` — closed/checkpointed process rule; proof-freeze matrix `doc/c-aol-harness-proof-freeze-matrix-v0-2026-04-28.md`.
- `CAOL-REAL-FIRE-SIGNAL-v0` and `CAOL-ROOF-HORDE-SPLIT-v0` — closed actual-playtest proof bundles for real-fire signal response and split-run roof-fire horde response.
- `CAOL-BANDIT-LIVE-SIGNAL-SITE-BOOTSTRAP-v0` — superseded partial lane; real player-fire signal response is closed by `CAOL-REAL-FIRE-SIGNAL-v0`, while remaining range/decay/scoring questions are future-only unless explicitly promoted.

---

## Documentation discipline

If the structure starts bloating again, apply this rule:
- `Plan.md` should be readable in a minute
- `TODO.md` should show only the current execution queue
- `TESTING.md` should show only current policy, latest relevant evidence, and pending probes
- `COMMIT_POLICY.md` should stop the dirty tree from becoming a lifestyle

If a sentence exists only to remember that something used to be true, it probably belongs in git history, not here.
