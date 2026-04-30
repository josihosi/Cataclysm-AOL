# Andi handoff: CAOL-WRITHING-STALKER-LIVE-FUN-SCENARIOS-v0

## Current canon state

`CAOL-WRITHING-STALKER-LIVE-FUN-SCENARIOS-v0` is the **ACTIVE / GREENLIT LIVE FUN-SCENARIO PLAYTEST PACKET**.

Authoritative canon is `Plan.md`, with `TODO.md`, `SUCCESS.md`, `TESTING.md`, and `doc/work-ledger.md` aligned downstream. This handoff is only a terse executor packet; if it disagrees with canon, repair it from canon.

Prior closed lanes that must not be reopened by drift: monster flavor/stat/spawn footing, old `CAOL-WRITHING-STALKER-v0`, `CAOL-WRITHING-STALKER-PATTERN-TESTS-v0`, roof-horde, Smart Zone, old fire proof lanes, and `CAOL-MULTI-CAMP-SIGNAL-GAUNTLET-v0`.

Queued-next visibility: `CAOL-ZOMBIE-RIDER-0.3-v0` is greenlit behind this active writhing-stalker live fun packet, with contract `doc/zombie-rider-0.3-initial-dev-packet-v0-2026-04-30.md` and benchmark suite `doc/zombie-rider-map-ai-funness-benchmark-suite-v0-2026-04-30.md`. Do **not** switch to it during the current active packet unless Schani/Josef explicitly promote it to active; it is listed here so release `0.3` prep does not disappear.

## Goal

Prove or falsify live player-facing fun for the writhing stalker: fair dread, readable counterplay, nasty repeated-attack rhythm, injured retreat, and no cheating.

Target feeling: the player was hunted, understood why, had tools, and the monster felt cunning rather than random.

## Canonical packet

- Contract: `doc/writhing-stalker-live-fun-scenarios-packet-v0-2026-04-30.md`
- Imagination source: `doc/writhing-stalker-live-fun-scenarios-imagination-source-of-truth-2026-04-30.md`
- Prior deterministic proof: `doc/writhing-stalker-behavior-pattern-proof-v0-2026-04-30.md`
- Prior live seam footing: `doc/writhing-stalker-playtest-ladder-v0-2026-04-30.md`

## Required scenario rows

### Scenario A — campfire / light counterplay

Proposed: `writhing_stalker.live_campfire_counterplay_mcw`.

Pass: light/focus causes hold/withdraw/cooling behavior and prevents unfair strike while the counterplay condition holds.

### Scenario B — alley predator / shadow route

Proposed: `writhing_stalker.live_alley_predator_mcw`.

Pass: cover/edge route beats open direct route, close strike is plausible, cooldown/reposition appears before any second strike, and there is no strike spam.

### Scenario C — zombie distraction without magic

Proposed: `writhing_stalker.live_zombie_distraction_mcw`.

Pass: zombie pressure helps only with plausible evidence/vulnerability; zombies alone do not grant magic target acquisition or strike permission.

### Scenario D — door/light escape

Proposed: `writhing_stalker.live_door_light_escape_mcw`.

Pass: readable escape/counterplay breaks pressure into hold/withdraw/cooldown instead of glued pursuit or stale destination chase.

### Scenario E — wounded predator retreat

Proposed: `writhing_stalker.live_wounded_predator_mcw`.

Pass: after legitimate pressure/attacks, badly-injured stalker withdraws, records HP/trigger, and does not snap back into strike.

## Evidence/reporting requirements

For each credited row, report:

- scenario/run id and artifact path;
- starting state: distance, evidence, light/focus, cover/direct-route availability, player vulnerability, stalker HP;
- compact per-turn/per-decision trace: decision, reason, route, distance, cooldown, strike count, HP band;
- pass/fail verdict and caveats;
- stability/perf: crash status, warnings/errors/log spam, and available `eval_us` or turn/cadence timing.

## Non-goals/cautions

- Do not claim fun from unit tests alone. This packet needs live-shaped scenario evidence.
- Staged-but-live is acceptable; fully natural random discovery is not required for v0.
- Do not make the monster scarier by making it omniscient.
- If a row breaks, preserve the red/yellow evidence and exact blocker. A truthful ugly report is useful; a fake green is just garnish on a lie.
