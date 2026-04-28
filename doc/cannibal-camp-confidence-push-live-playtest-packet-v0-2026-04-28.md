# Cannibal camp confidence-push live playtest packet v0 - 2026-04-28

Status: GREENLIT / QUEUED CONFIDENCE-PUSH PLAYTEST

Imagination source: `doc/cannibal-camp-confidence-push-live-playtest-imagination-source-of-truth-2026-04-28.md`

## Contract

The closed `Cannibal camp night-raid behavior packet v0` already has deterministic support and a green live matrix for the named scenarios. This packet is a confidence uplift after Josef greenlit pushing Andi's playtesting harder: broaden the live proof shape enough to raise trust that the cannibal behavior works in play, not only inside the closed matrix.

This is queued work. It must not interrupt the active bandit camp-map lane or any explicitly promoted harness-review lane.

## Scope

1. Build a small live/harness playtest family that exercises the already-finished cannibal behavior in broader shapes:
   - wandering day pressure near a cannibal camp;
   - too-close night/concealment contact;
   - save/reload continuation of an active cannibal group;
   - at least one different-seed or different-footing repeat;
   - a small bandit contrast control for shakedown/pay/fight separation.
2. Use proof-freeze step ledgers for every meaningful live action and wait/time-passage step.
3. Record decisive structured fields: scenario path, commit/runtime match, profile/world provenance, active site/profile, signal/lead source, posture/job, pack size, darkness/concealment, current/recent exposure, shakedown/combat-forward flags, active member IDs, target state, save/reload state, and repeat/control verdict.
4. Keep deterministic tests as support only unless a regression is discovered.
5. Classify the result as confidence uplift, not feature redesign.

## Non-goals

- No new cannibal behavior design unless the confidence push finds a concrete bug or tuning gap.
- No reopening the closed cannibal packet as failed merely because broader confidence proof is useful.
- No giant wandering manual session with vague vibes and no artifacts.
- No startup/load-only closure.
- No treating a pre-seeded fixture state as proof of live dispatch/local-contact behavior.
- No interrupting the active bandit camp-map lane unless Schani/Josef explicitly promotes this packet.

## Proposed test family

### 1. Wandering day pressure

Player starts near enough to a cannibal camp to create smoke/routine/noise pressure over bounded time without forced local contact.

Expected green shape:
- real live dispatch/signal path reached;
- `profile=cannibal_camp`;
- stalk/probe/hold posture by day;
- no instant combat-forward behavior unless the setup genuinely escalates;
- no shakedown/pay/fight surface.

### 2. Night mistake / contact

Player gets too close or visible at night / under concealment after the camp has a plausible pressure reason.

Expected green shape:
- real local-contact path reached;
- darkness/concealment participates in the decision;
- multi-member pack attack can occur;
- no bandit toll/shakedown surface.

### 3. Reload brain

Create an active cannibal group, save, reload without fixture reinstall, then advance bounded time.

Expected green shape:
- active group/profile/member IDs/target state persist;
- post-reload behavior remains coherent;
- no profile or target lobotomy.

### 4. Different-seed repeat

Repeat one of the day/night shapes on a different camp position, member count, or save footing.

Expected green shape:
- same broad behavior class appears without depending on the original McWilliams geometry;
- fixture provenance is explicit.

### 5. Bandit contrast control

Run a similar contact/control shape for a bandit camp.

Expected green shape:
- bandit shakedown/pay/fight surface remains available where appropriate;
- cannibal no-shakedown result is not just a global dialogue break.

## Success state

- [ ] A compact live/harness matrix records the five proposed shapes or names which could not be honestly staged.
- [ ] At least one broader wandering/day pressure run reaches the real live dispatch/signal path and supports the closed cannibal behavior claim.
- [ ] At least one night/contact run reaches real local-contact behavior and supports pack-forward danger under darkness/concealment.
- [ ] Save/reload continuation proves active cannibal group/profile/target/job state stays coherent after reload.
- [ ] A different-seed/different-footing run reduces fixture-bias risk.
- [ ] A bandit contrast control preserves the shakedown/pay/fight distinction.
- [ ] The final verdict updates confidence honestly: higher confidence, green with watchlist, blocked by harness/fixture gap, or red with a specific bug/tuning gap.

## Validation gates

Minimum expected gates when implemented:

- `git diff --check`;
- current-runtime build if runtime/harness code changed;
- relevant focused `[bandit][live_world][cannibal]` / `[bandit][live_world]` deterministic tests if code changed;
- named live/harness scenario reports with all-green step ledgers for credited feature-path claims;
- compact matrix doc or section with decisive fields and artifact paths, not screenshot soup.
