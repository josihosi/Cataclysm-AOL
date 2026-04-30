# Andi handoff: CAOL-WRITHING-STALKER-LIVE-FUN-SCENARIOS-v0

## Current canon state

`CAOL-WRITHING-STALKER-LIVE-FUN-SCENARIOS-v0` is **CLOSED / CHECKPOINTED GREEN V0**.

Authoritative canon is `Plan.md`, with `TODO.md`, `SUCCESS.md`, `TESTING.md`, and `doc/work-ledger.md` aligned downstream. This handoff is only a terse executor packet; if it disagrees with canon, repair it from canon.

Prior closed lanes that must not be reopened by drift: monster flavor/stat/spawn footing, old `CAOL-WRITHING-STALKER-v0`, `CAOL-WRITHING-STALKER-PATTERN-TESTS-v0`, roof-horde, Smart Zone, old fire proof lanes, and `CAOL-MULTI-CAMP-SIGNAL-GAUNTLET-v0`.

Queued-next visibility: `CAOL-ZOMBIE-RIDER-0.3-v0` is greenlit behind the now-closed writhing-stalker live fun packet, with contract `doc/zombie-rider-0.3-initial-dev-packet-v0-2026-04-30.md` and benchmark suite `doc/zombie-rider-map-ai-funness-benchmark-suite-v0-2026-04-30.md`. Promote deliberately before starting so the active lane boundary stays visible.

## Closure proof

Proof doc: `doc/writhing-stalker-live-fun-scenario-proof-v0-2026-04-30.md`.

Green v0 target feeling: the player was hunted, understood why, had tools, and the monster felt cunning rather than random.

## Canonical packet

- Contract: `doc/writhing-stalker-live-fun-scenarios-packet-v0-2026-04-30.md`
- Imagination source: `doc/writhing-stalker-live-fun-scenarios-imagination-source-of-truth-2026-04-30.md`
- Prior deterministic proof: `doc/writhing-stalker-behavior-pattern-proof-v0-2026-04-30.md`
- Prior live seam footing: `doc/writhing-stalker-playtest-ladder-v0-2026-04-30.md`

## Credited scenario rows

### Scenario A — campfire / light counterplay

`writhing_stalker.live_campfire_counterplay_mcw` -> `.userdata/dev-harness/harness_runs/20260430_233129/`: light/focus causes withdraw/cooling and prevents unfair strike.

### Scenario B — alley predator / shadow route

`writhing_stalker.live_alley_predator_mcw` -> `.userdata/dev-harness/harness_runs/20260430_233156/`: cover-shadow route, plausible close strike, cooldown/reposition, no spam.

### Scenario C — zombie distraction without magic

`writhing_stalker.live_zombie_distraction_mcw` -> `.userdata/dev-harness/harness_runs/20260430_233521/`: zombie pressure helps only with plausible evidence/vulnerability. `writhing_stalker.live_zombie_distraction_no_magic_guard_mcw` -> `.userdata/dev-harness/harness_runs/20260430_233335/`: zombies alone do not grant magic target acquisition through the wall.

### Scenario D — door/light escape

`writhing_stalker.live_door_light_escape_mcw` -> `.userdata/dev-harness/harness_runs/20260430_233405/`: readable light/focus escape breaks pressure into withdraw/cooling instead of strike/glued pursuit.

### Scenario E — wounded predator retreat

`writhing_stalker.live_wounded_predator_mcw` -> `.userdata/dev-harness/harness_runs/20260430_233434/`: badly-injured stalker withdraws and does not snap back into strike.

## Evidence/reporting requirements

Already satisfied in the proof doc. For any future reopened stricter row, report:

- scenario/run id and artifact path;
- starting state: distance, evidence, light/focus, cover/direct-route availability, player vulnerability, stalker HP;
- compact per-turn/per-decision trace: decision, reason, route, distance, cooldown, strike count, HP band;
- pass/fail verdict and caveats;
- stability/perf: crash status, warnings/errors/log spam, and available `eval_us` or turn/cadence timing.

## Non-goals/cautions

- Do not claim fun from unit tests alone. This packet needs live-shaped scenario evidence.
- Staged-but-live is acceptable; fully natural random discovery is not required for v0.
- Do not make the monster scarier by making it omniscient.
- If a future reopened row breaks, preserve the red/yellow evidence and exact blocker. A truthful ugly report is useful; a fake green is just garnish on a lie.
