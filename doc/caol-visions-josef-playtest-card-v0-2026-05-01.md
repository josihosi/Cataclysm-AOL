# C-AOL visions — Josef playtest card v0 (2026-05-01)

Status: AGENT-SIDE SAMPLER DRAFT / HANDOFF COMMANDS READY / JOSEF TASTE NOT YET COLLECTED

Contract: `doc/caol-visions-playtest-sampler-packet-v0-2026-05-01.md`.
Imagination source: `doc/caol-visions-playtest-imagination-source-2026-05-01.md`.

This is a small taste sampler, not a proof ledger. The proof footing is cited so Josef does not have to inspect it. Josef's job is to play/look for a few minutes and answer whether the vision feels fun, fair, readable, optical, alive, and gnostic.

## v0 selection

Chosen postcards:

1. Writhing stalker — zombie-shadow predator.
2. Zombie rider — late-world mounted terror with cover/spacing counterplay.
3. Flesh raptor — circling skirmisher vs old stab/flee annoyance.
4. Camp locker — practical camp usefulness without inventory soup.

Deferred from v0: bandit/basecamp pressure. Reason: bandit scenic chat openings are already a separate queued lane, and the older optical snapshot includes version/staging caveats. Keep bandit as v1 unless Josef specifically wants living-world human pressure in this sampler now.

## Shared boundary

- These are staged/handoff scenes, not natural random-discovery proof.
- Do not treat a bad taste answer as reopening the old closed v0 automatically; it becomes a follow-up packet if Josef wants it.
- Do not ask Josef to read debug logs during play. Logs are only agent-side footing.
- Screenshot checkpoints are optical/taste evidence, not substitutes for mechanical proof.

Runtime screenshot review folder prepared from existing artifacts: `/Users/josefhorvath/.openclaw/workspace/runtime/caol-visions-card-review-20260502/`.

Post-crunch hygiene note: before any cited row becomes Josef-facing optical footing, grep its logs for runtime `ERROR GAME` / warnings. This sampler is for taste, not proof archaeology; dirty footing must be explicit or replaced.

---

## 1. Writhing stalker — zombie-shadow predator

**Vision:** the stalker should feel like it uses zombie chaos and shadows to choose a cruel cutoff angle, not like an omniscient cheating goblin.

**Handoff/setup:**

Primary clean footing row:

```sh
python3 tools/openclaw_harness/startup_harness.py handoff writhing_stalker.live_escape_side_zombie_retreat_mcw
```

Secondary / caveated quiet-side row, only if rerun clean or explicitly labelled dirty:

```sh
python3 tools/openclaw_harness/startup_harness.py handoff writhing_stalker.live_quiet_side_zombie_pressure_mcw
```

**Try for 2-5 minutes:** move cautiously around the staged zombie pressure; watch whether the stalker appears to pressure the quiet/escape side rather than beelining through everything. Use light/space if you want to test whether it still feels readable.

**Screenshot checkpoint:** capture the moment after the retreat-side setup where zombies are one visible/front pressure side and the stalker threat/cutoff angle is readable or suspicious. Primary existing artifact screenshot: `.userdata/dev-harness/harness_runs/20260501_071940/advance_escape_side_zombie_retreat_window.after.png` (copy: `01-stalker-escape-side-primary.png`). Expected visible fact: the map state should support “zombies pressure one side; stalker pressure/cutoff reads as the escape-side/quiet-side angle,” or else be marked optically ambiguous.

Caveated old screenshot: `.userdata/dev-harness/harness_runs/20260501_071548/advance_quiet_side_zombie_pressure_window.after.png` (copy: `01-stalker-quiet-side.png`) is **not clean primary footing**. Its artifact log contains `ERROR GAME ... writhing stalker can't move to its location! ... reinforced white concrete wall`, so use it only as a dirty/illustrative artifact unless rerun clean.

**Taste questions:**

- Does the stalker feel like it is using the living mess around you, or like the game is cheating?
- Is the counterplay visible enough before the danger lands?
- Does the optical read match the mechanical idea of quiet-side pressure?
- Does it carry gnostic wrongness — something watching the pattern — or just a pathfinding trick?
- Would this make you want another turn, or just make you sigh?

**Agent footing:** primary row `writhing_stalker.live_escape_side_zombie_retreat_mcw` -> `.userdata/dev-harness/harness_runs/20260501_071940/`; targeted grep found no `ERROR GAME` / warning backtrace in the cited `debug.final.log`, `probe.report.json`, or `probe.artifacts.log` excerpts. Proof note: `doc/writhing-stalker-zombie-shadow-live-escape-side-proof-v0-2026-05-01.md`.

Dirty/caveated secondary row: `writhing_stalker.live_quiet_side_zombie_pressure_mcw` -> `.userdata/dev-harness/harness_runs/20260501_071548/`; proof note `doc/writhing-stalker-zombie-shadow-live-quiet-side-proof-v0-2026-05-01.md`. Caveat: targeted grep found `ERROR GAME` backtrace text in `probe.artifacts.log`: `writhing stalker can't move to its location! ... reinforced white concrete wall`. Do not relay that row as clean Josef-facing optical footing without a fresh clean rerun.

**Do not overinterpret:** this proves/tastes local staged zombie-shadow behavior, not natural overmap stalking or every lighting situation.

---

## 2. Zombie rider — late-world mounted terror

**Vision:** a mounted archer should read as terrifying late-world pressure: shooting when it has line of fire, bunny-hopping/repositioning when too close, and respecting cover instead of becoming a polite indoor statue.

**Handoff/setup:**

```sh
python3 tools/openclaw_harness/startup_harness.py handoff zombie_rider.live_open_field_pressure_mcw
```

Optional counterplay contrast:

```sh
python3 tools/openclaw_harness/startup_harness.py handoff zombie_rider.live_cover_escape_mcw
```

**Try for 2-5 minutes:** in the open-field row, watch whether the rider's distance/positioning and attack pressure are readable. Then try to break line of fire or close distance. In the cover row, check whether cover feels like real counterplay.

**Screenshot checkpoint:** capture the rider at visible pressure range or immediately after close repositioning. Existing artifact screenshot: `.userdata/dev-harness/harness_runs/20260502_050055/advance_rider_open_field_pressure_window.after.png` (copy: `02-rider-open-field.png`). Expected visible fact: the rider should be visually locatable as a pressure source with enough map context to judge whether the movement/spacing reads as mounted archer behavior.

**Taste questions:**

- Does this feel like a scary rider, or like a gimmick with a bow?
- Is the shot/reposition rhythm visually legible enough to learn from?
- Does cover/LOS counterplay feel fair, or too opaque?
- Does the rider carry late-world omen/terror, or just another monster stat block?
- Is the pressure exciting, annoying, too weak, or too lethal?

**Agent footing:** current close-pressure proof `zombie_rider.live_open_field_pressure_mcw` -> `.userdata/dev-harness/harness_runs/20260502_050055/`, with audited ammo, `aggro_before=no aggro_after=yes`, bow ammo decrement, and `too_close_bunny_hop` reposition. Proof: `doc/zombie-rider-close-pressure-no-attack-proof-v0-2026-05-02.md`. Cover footing: `.userdata/dev-harness/harness_runs/20260501_021656/`, proof `doc/zombie-rider-live-funness-cover-escape-proof-v0-2026-05-01.md`.

**Do not overinterpret:** staged McWilliams feature proof, not natural discovery, full siege navigation, or final late-game balance.

---

## 3. Flesh raptor — circling skirmisher

**Vision:** the raptor should feel like a readable circling/skirmishing predator, not a prettier version of the old stab/flee equipment-damage pest.

**Handoff/setup:**

```sh
python3 tools/openclaw_harness/startup_harness.py handoff flesh_raptor.live_equipment_frustration_comparison_mcw
```

Optional pure movement rows:

```sh
python3 tools/openclaw_harness/startup_harness.py handoff flesh_raptor.live_open_field_skirmisher_mcw
python3 tools/openclaw_harness/startup_harness.py handoff flesh_raptor.live_crowded_arc_skirmisher_mcw
```

**Try for 2-5 minutes:** let the raptor circle and commit a few swoops. Watch whether the orbit/swoop cadence is readable and whether the combat consequences feel like fair danger instead of random annoyance.

**Screenshot checkpoint:** capture either the raptor positioned off-angle before a swoop or the message history after the first consequence. Existing artifact screenshot: `.userdata/dev-harness/harness_runs/20260501_062300/open_message_history_after_raptor_comparison_window.after.png` (copy: `03-raptor-message-history.png`). Expected visible fact: player-facing text should show flesh-raptor consequence/frustration (`flesh-raptor`, `impales`, `cut`, bleeding), while the play session should let Josef judge whether the visible predator motion makes that consequence feel earned.

**Taste questions:**

- Is the raptor fun-readable as a skirmisher, or still old stab/flee annoyance with extra steps?
- Do the orbit and swoop timings give enough warning?
- Are equipment/body consequences scary but fair, or just irritating?
- Does the creature feel organically predatory and weird, or like debug behavior exposed?
- Would you keep fighting, flee, or reload out of annoyance?

**Agent footing:** `flesh_raptor.live_equipment_frustration_comparison_mcw` -> `.userdata/dev-harness/harness_runs/20260501_062300/`; open/crowded/corridor rows -> `.userdata/dev-harness/harness_runs/20260501_052709/`, `20260501_053414/`, `20260501_054807/`. Contract/closure evidence: `doc/flesh-raptor-circling-skirmisher-packet-v0-2026-05-01.md` and `TESTING.md` closed receipt.

**Do not overinterpret:** no new equipment-damage tuning was done in the raptor v0; this is a taste comparison against the old-feeling frustration bar.

---

## 4. Camp locker — practical camp usefulness

**Vision:** camp should feel more capable and legible: a real `CAMP_LOCKER` zone exists, can be created/reopened, and can feed practical gear service without magic inventory soup.

**Handoff/setup:**

For visible Zone Manager/readability:

```sh
python3 tools/openclaw_harness/startup_harness.py handoff locker.zone_manager_save_probe_mcw
```

For practical cold-weather service:

```sh
python3 tools/openclaw_harness/startup_harness.py handoff locker.weather_wait
```

**Try for 2-5 minutes:** open/inspect Zone Manager and locker state; in the weather row, inspect whether NPC gear usefulness is believable after cold-service turns rather than feeling like invisible automation.

**Screenshot checkpoint:** capture Zone Manager with `Basecamp: Locker` / `Probe Locker` as `CAMP_LOCKER`, or the worker/equipment state after service. Existing artifact screenshot: `.userdata/dev-harness/harness_runs/20260502_041828/reopen_zones_manager_for_persistence_check.after.png` (copy: `04-locker-zone-manager.png`). Expected visible fact: camp locker should be visible as a real zone/UI concept, not only a debug-log service.

**Taste questions:**

- Does the camp feel more practically useful, or still like paperwork with legs?
- Is the locker zone UI/readability enough to understand what happened?
- Does gear service feel plausible from camp stock, or too magical/invisible?
- Does this carry C-AOL camp-life meaning — people surviving together with systems — or only utility plumbing?
- What one UI/message cue would make it more legible?

**Agent footing:** `locker.zone_manager_save_probe_mcw` -> `.userdata/dev-harness/harness_runs/20260502_041828/`; `locker.weather_wait` -> `.userdata/dev-harness/harness_runs/20260502_041300/`; proof `doc/camp-locker-zone-playtest-proof-v0-2026-05-02.md`.

**Do not overinterpret:** Robbie package e2e remained blocked/no-credit for that lane; v0 camp locker proof covers bounded zone persistence and practical service, not the entire basecamp UX.

---

## How to record Josef answers

For each postcard, collect:

- Optical read: clear / ambiguous / wrong.
- Mechanical read: fun-readable / fake / unfair / invisible / annoying.
- Gnostic read: has inner C-AOL wrongness/meaning / hollow mechanic / unsure.
- One most-wanted change, if any.
- Whether this should become a follow-up packet.

Short Josef-facing ask:

> Please try any 2-4 postcards for 2-5 minutes each. For each, take or point to one screenshot moment and answer: did it read optically, mechanically, and gnostically; was it fair/fun/alive; and what felt fake or hollow?
