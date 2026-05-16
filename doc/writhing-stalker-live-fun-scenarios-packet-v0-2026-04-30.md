# CAOL-WRITHING-STALKER-LIVE-FUN-SCENARIOS-v0 — live fun-scenario benchmark packet

## Classification

**ACTIVE / GREENLIT LIVE FUN-SCENARIO PLAYTEST PACKET**

Josef explicitly greenlit turning the writhing-stalker fun readout into live scenarios: “YESSSS DO IT”. This packet follows the closed deterministic `CAOL-WRITHING-STALKER-PATTERN-TESTS-v0`; it does not reopen that closure, but asks whether the same behavior feels fun/fair in live-shaped scenes.

Imagination source: `doc/writhing-stalker-live-fun-scenarios-imagination-source-of-truth-2026-04-30.md`.

Prior deterministic proof: `doc/writhing-stalker-behavior-pattern-proof-v0-2026-04-30.md`.

## Goal

Prove or falsify live player-facing fun for the writhing stalker: fair dread, readable counterplay, nasty repeated-attack rhythm, injured retreat, and no cheating.

The target experience is:

> I was hunted, I understood why, I had tools, and the monster felt cunning rather than random.

## Required scenario rows

Equivalent names are acceptable if the packet records the final names and reasons.

### Scenario A — campfire / light counterplay

Proposed name: `writhing_stalker.live_campfire_counterplay_mcw`.

Setup:
- player in or near bright/focused light;
- stalker has plausible local footing but is exposed or watched;
- enough turns pass for the live plan to choose.

Pass:
- live decision is hold/withdraw/cooling behavior, not suicide charge;
- evidence names brightness/focus as the reason;
- no strike occurs while the counterplay condition holds.

Fail:
- direct charge/strike through bright focused counterplay without a forced/no-cover explanation;
- no visible/logged reason tying the behavior to light/focus.

Fun reason: light and awareness must be player tools, not decoration.

### Scenario B — alley predator / shadow route

Proposed name: `writhing_stalker.live_alley_predator_mcw`.

Setup:
- cover/edge route and open/direct route both exist;
- player is plausibly evidenced and vulnerable enough for stalking pressure;
- bounded turn/wait sequence records path/decision over multiple turns.

Pass:
- stalker shadows via cover/edge instead of open beeline when available;
- strike only happens from a plausible close/opportunity state;
- after strike, cooldown/reposition appears before another strike.

Fail:
- open direct route chosen despite available cover without forced-no-cover state;
- immediate repeat strike spam;
- one-and-done forever while healthy and the player remains vulnerable.

Fun reason: the player should feel hunted by terrain-aware pressure, not chased by a straight-line Roomba with teeth.

### Scenario C — zombie distraction without magic

Proposed name: `writhing_stalker.live_zombie_distraction_mcw`.

Setup:
- zombies pressure or distract the player;
- stalker has only plausible local evidence, not omniscient global knowledge;
- include a contrast or guardrail where zombie pressure exists but player vulnerability/evidence is insufficient.

Pass:
- zombie pressure increases opportunity only when the player is actually vulnerable/evidenced;
- zombies alone do not grant magic strike permission;
- trace/log shows why the strike is or is not legal.

Fail:
- zombie presence alone causes unexplained target acquisition/strike;
- the stalker ignores an actually vulnerable distracted player in a plausible close state without a reason.

Fun reason: fights should become dangerous without turning every zombie into a psychic radio tower.

### Scenario D — door/light escape

Proposed name: `writhing_stalker.live_door_light_escape_mcw`.

Setup:
- stalker has pressure and/or recent strike footing;
- player breaks line, reaches light, shuts a door, gains focus, or otherwise creates readable safety;
- bounded turns pass after the escape move.

Pass:
- stalker drops to hold/withdraw/cooling/off-pressure behavior rather than glued pursuit;
- evidence shows the counterplay lever that changed behavior;
- no stale destination keeps dragging it through safety counterplay.

Fail:
- glued pursuit after evidence/counterplay should have broken pressure;
- no readable distinction between exposed player and escaped/focused player.

Fun reason: dread needs agency. If escape tools do nothing, it becomes tax paperwork with bleeding.

### Scenario E — wounded predator retreat

Proposed name: `writhing_stalker.live_wounded_predator_mcw`.

Setup:
- stalker gets one or more legitimate attacks while healthy if possible;
- player damages it into the badly-injured band;
- bounded turns pass afterward.

Pass:
- badly injured stalker withdraws even if the player remains vulnerable;
- retreat trigger and HP band are recorded;
- no immediate strike-after-withdraw snapback.

Fail:
- stalker fights to the death through badly-injured state;
- flees from trivial damage before creating pressure;
- withdraw -> strike jitter appears.

Fun reason: it should feel self-preserving and alive, not heroic, cowardly, or broken.

## Metrics / evidence to report

For each credited row, report:

- scenario name and run id/path;
- starting state: player/stalker distance, evidence source, light/focus state, cover/direct route availability, player vulnerability flags, stalker HP;
- compact per-turn or per-decision trace: decision, reason, route, distance, cooldown, strike count, HP band;
- pass/fail verdict against that scenario’s row;
- any screenshot/OCR/state artifact if the row claims visible player-facing state;
- stability: crash status, runtime warnings/errors, debug-log spam, and available `eval_us` or turn/cadence timing.

## Non-goals

- Do not redesign the writhing stalker into a full nemesis-memory system.
- Do not reopen monster flavor/stat/spawn footing, deterministic pattern closure, roof-horde, Smart Zone, old fire proof lanes, or the multi-camp gauntlet.
- Do not claim fun from unit tests alone; this packet needs live-shaped scenario evidence.
- Do not require fully natural random discovery for v0; staged-but-live scenes are acceptable if live plan/turn behavior is honestly exercised.
- Do not make the monster scarier by making it omniscient.

## Success state

- Scenario A campfire/light counterplay is green or honestly red/yellow with exact blocker.
- Scenario B alley predator/shadow route is green or honestly red/yellow with exact blocker.
- Scenario C zombie distraction without magic is green or honestly red/yellow with exact blocker.
- Scenario D door/light escape is green or honestly red/yellow with exact blocker.
- Scenario E wounded predator retreat is green or honestly red/yellow with exact blocker.
- At least one repeated-turn trace proves or falsifies the fun rhythm: shadow/pressure -> strike -> cooldown/reposition -> possible second strike -> injured retreat or escape break.
- All credited rows include scenario/run ids, decision/reason traces, pass/fail verdicts, and stability/perf notes.
- If behavior/tuning change is needed, it is the smallest change that preserves no-omniscience, counterplay, cooldown anti-spam, and injured retreat.
- `Plan.md`, `TODO.md`, `SUCCESS.md`, `TESTING.md`, `doc/work-ledger.md`, and `andi.handoff.md` match final state.
