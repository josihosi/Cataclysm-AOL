# CAOL Josef playtest save-pack card v0 (2026-05-02)

Status: PARTIAL JOSEF-FACING CARD / FOUR READY ROWS / CONTRAST + BANDIT + RAPTOR CAVEATED

Contract: `doc/caol-josef-playtest-save-pack-packet-v0-2026-05-02.md`.
Handoff packet: `doc/caol-josef-playtest-save-pack-handoff-v0-2026-05-02.md`.

This card is the usable index, not a proof pantry. The fresh pass produced four ready staged rows and several honest no-credit/caveated rows. The missing rows are not hidden: flesh raptor needs a fresh clean current-build row, bandit needs a fresh contrast/preflight row, and the stalker contrast matrix is not complete. Na bravo, the soup has labels now.

## How to use this card

Run commands from the repo root. The probe rows below are the evidence runs; use the matching `handoff` command when Josef wants an enterable live session left open instead of an auto-closed probe.

Shared caveat for every ready row: these are staged harness scenes on McWilliams/Sandy Creek footing, not natural random-discovery proof, release packaging, or final balance. Josef's job is taste: readable, fair, fun, alive, gnostic, fake, hollow, annoying.

All fresh rows inspected here report `portal_storm_warning.classification = clear` / `status = not_observed`; no silent portal-storm contamination was observed.

## Ready entries

### 1. `/horde/camp` — Cannibal camp night pressure

- **Use:**
  ```sh
  python3 tools/openclaw_harness/startup_harness.py handoff cannibal.live_world_night_local_contact_pack_mcw
  ```
- **Evidence run:** `.userdata/dev-harness/harness_runs/20260502_125033/`.
- **Save/world footing:** McWilliams via `.userdata/dev-harness/save/McWilliams`.
- **Current evidence status:** GREEN feature-path probe: `feature_proof=true`, `verdict=artifacts_matched`, `step_ledger_summary.status=green_step_local_proof`, `2/2` green rows, runtime warnings `[]`, portal-storm classification `clear`.
- **What to try for 2-5 minutes:** let the contact unfold, then move/retreat enough to see whether the cannibal pressure reads like a killing pack rather than bandit negotiation theater.
- **Expected read:** night/local contact produces cannibal `attack_now` pressure with `pack_size=2`, `darkness_or_concealment=yes`, `local_contact=yes`, `shakedown=no`, and `combat_forward=yes`.
- **Taste questions:**
  - Does this feel like a cannibal camp mistake turning lethal, or just two spawned enemies?
  - Is the no-negotiation distinction from bandits readable?
  - Does the pressure feel scary-fair, or cheap?
  - Does it have camp/horde wrongness, or just combat bookkeeping?
- **Caveat:** staged local-contact row; it does not prove broad natural cannibal discovery, every camp roster shape, or long-form pursuit.

### 2. `/rider/stalker/raptor` — Zombie rider open-field predator/counterplay

- **Use:**
  ```sh
  python3 tools/openclaw_harness/startup_harness.py handoff zombie_rider.live_open_field_pressure_mcw
  ```
- **Evidence run:** `.userdata/dev-harness/harness_runs/20260502_125150/`.
- **Save/world footing:** McWilliams via `.userdata/dev-harness/save/McWilliams`.
- **Current evidence status:** GREEN feature-path probe: `feature_proof=true`, `verdict=artifacts_matched`, `step_ledger_summary.status=green_step_local_proof`, `6/6` green rows, runtime warnings `[]`, portal-storm classification `clear`.
- **What to try for 2-5 minutes:** stay in the open long enough to read bow pressure, then close distance or break line of fire to judge whether the rider feels like a mounted archer instead of a polite statue.
- **Expected read:** the rider reaches `decision=bow_pressure reason=line_of_fire`, flips `aggro_before=no aggro_after=yes`, consumes arrows, then uses cooldown/too-close repositioning such as `too_close_bunny_hop`.
- **Taste questions:**
  - Does it feel like late-world mounted terror, or just a zombie with a gimmick bow?
  - Is shot/reposition timing visible enough to learn?
  - Does cover/spacing counterplay feel fair?
  - Is it exciting pressure or irritating poke-kiting?
- **Caveat:** staged McWilliams feature proof; not natural spawn/discovery, full siege navigation, or final late-game balance.

### 3. `/rider/stalker/raptor` — Writhing stalker hit-fade retreat rhythm

- **Use:**
  ```sh
  python3 tools/openclaw_harness/startup_harness.py handoff writhing_stalker.live_hit_fade_retreat_mcw
  ```
- **Evidence run:** `.userdata/dev-harness/harness_runs/20260502_125231/`.
- **Save/world footing:** McWilliams via `.userdata/dev-harness/save/McWilliams`.
- **Current evidence status:** GREEN feature-path probe: `feature_proof=true`, `verdict=artifacts_matched`, `step_ledger_summary.status=green_step_local_proof`, `6/6` green rows, runtime warnings `[]`, portal-storm classification `clear`.
- **What to try for 2-5 minutes:** let the stalker close and hit; watch whether it bursts, withdraws, and cools off instead of doing one-hit generic flee or endless melee spam. Then add light/attention if you want to test the feel, but do not treat that as the proven contrast row.
- **Expected read:** live plan shows `decision=strike`, `decision=withdraw`, `decision=cooling_off`, burst accounting around `burst=0/2`, `1/2`, `2/2`, and `retreat_distance=8`.
- **Taste questions:**
  - Does the hit-fade rhythm breathe, or does it still feel like a cheap goblin in the wall?
  - Is the retreat readable enough to be fair?
  - Does the danger feel like dread with counterplay, or just intermittent damage?
  - Would light/allies make you expect a different behavior?
- **Caveat:** staged hit-fade proof only. The broader stalker contrast pass is still caveated below.

### 4. `/visions/camp-locker` — Camp locker practical usefulness

- **Use:**
  ```sh
  python3 tools/openclaw_harness/startup_harness.py handoff locker.weather_wait
  ```
  Optional Zone Manager visibility footing from the closed locker proof:
  ```sh
  python3 tools/openclaw_harness/startup_harness.py handoff locker.zone_manager_save_probe_mcw
  ```
- **Evidence run:** `.userdata/dev-harness/harness_runs/20260502_125339/` for `locker.weather_wait`; prior zone footing `.userdata/dev-harness/harness_runs/20260502_041828/`.
- **Save/world footing:** Sandy Creek via `.userdata/dev-harness/save/Sandy Creek` for the fresh weather row.
- **Current evidence status:** GREEN feature-path probe: `feature_proof=true`, `verdict=artifacts_matched`, `step_ledger_summary.status=green_step_local_proof`, `3/3` green rows, runtime warnings `[]`, portal-storm classification `clear`.
- **What to try for 2-5 minutes:** inspect the worker/camp state after the cold-service window; optionally open Zone Manager and inspect `CAMP_LOCKER` visibility/persistence in the zone row.
- **Expected read:** camp locker service uses real locker-zone stock: `camp locker: queued`, `plan for`, `after`, and `serviced`; decisive line includes `locker_tiles=1`, `candidates=1`, `changed_slots=1`, `applied=true`, with the worker ending in `cargo pants` and old `antarvasa` returned to stock.
- **Taste questions:**
  - Does the camp feel practically more alive, or still like invisible paperwork?
  - Is the locker/zone relationship legible enough?
  - Does gear service feel plausible from communal stock?
  - What one UI or message cue would make this less magical?
- **Caveat:** bounded zone/service proof, not a full basecamp UX redesign. Robbie package e2e remains no-credit from the locker lane.

## Blocked or caveated entries

### 5. `/rider/stalker/raptor` — Flesh raptor skirmisher row is not fresh-ready

- **Best caveated command if Josef explicitly wants the old taste row:**
  ```sh
  python3 tools/openclaw_harness/startup_harness.py handoff flesh_raptor.live_equipment_frustration_comparison_mcw
  ```
- **Older green footing:** `.userdata/dev-harness/harness_runs/20260501_062300/`, with open/crowded/corridor support from `20260501_052709`, `20260501_053414`, and `20260501_054807`; closure doc `doc/flesh-raptor-circling-skirmisher-packet-v0-2026-05-01.md`.
- **Fresh save-pack attempts:**
  - `.userdata/dev-harness/harness_runs/20260502_125055/` -> `blocked_flesh_raptor_open_field_live_plan_missing`.
  - `.userdata/dev-harness/harness_runs/20260502_125447/` -> `blocked_flesh_raptor_open_field_live_plan_missing`.
- **Portal-storm status:** clear/not observed in both fresh blocked rows.
- **Why blocked:** screen capture failed on the live turn window and the same-run raptor live-plan metadata was missing (`missing_required_items`, `metadata_abort_guard`). The first run at least loaded and proved the saved noon/raptor preconditions; neither fresh row is feature proof.
- **Expected read from older footing only:** orbit/swoop cadence, under-occupied arc preference, and equipment/frustration consequences should be taste-judged, but this is not a fresh current save-pack green row.
- **Taste questions when rerun clean:** does it read as a circling predator rather than stab/flee irritation; are swoops telegraphed; are equipment/body consequences fair; would this make Josef keep playing or reload in annoyance?
- **Next honest step:** rerun or repair the raptor save-pack row until screen capture and live-plan artifact proof both go green, or explicitly deliver it as an older-footing taste-only row.

### 6. `/rider/stalker/raptor` — Writhing stalker contrast pass is incomplete

- **Ready no-fire/low-threat-ish footing:** `writhing_stalker.live_hit_fade_retreat_mcw` -> `.userdata/dev-harness/harness_runs/20260502_125231/` is green as listed above.
- **Fire/light contrast attempt:** `.userdata/dev-harness/harness_runs/20260502_125651/` (`writhing_stalker.live_campfire_counterplay_mcw`) is blocked: `blocked_audit_writhing_stalker_campfire_counterplay_live_plan_missing`, screen capture failed, live-plan metadata missing. Portal-storm classification clear.
- **Zombie/escape-side pressure attempt:** `.userdata/dev-harness/harness_runs/20260502_125716/` (`writhing_stalker.live_escape_side_zombie_retreat_mcw`) is mostly useful but not green: six of seven ledger rows are green, and same-run metadata proves escape-side cutoff/strike/withdraw/cooling-off; one live-window screen-capture row is red, so the report verdict is `blocked_step_local_proof`. Portal-storm classification clear.
- **Time setup failed rerun:** `.userdata/dev-harness/harness_runs/20260502_125633/` blocked on missing midnight setup plus screen capture failure.
- **Taste value:** the green hit-fade row is playable now; the escape-side row can inform agent reasoning but should not be sold as a clean optical card. The fire/light and high-threat contrast matrix still needs a fresh clean pass.

### 7. `/horde/camp` — Bandit pressure / shakedown / basecamp contact is not save-pack ready

- **Best existing footing, not fresh contrast closure:**
  ```sh
  python3 tools/openclaw_harness/startup_harness.py handoff bandit.extortion_first_demand_pay_mcw
  python3 tools/openclaw_harness/startup_harness.py handoff bandit.extortion_reopened_demand_mcw
  python3 tools/openclaw_harness/startup_harness.py handoff bandit.extortion_at_camp_standoff_mcw
  ```
- **Existing green docs/runs:** `doc/bandit-scenic-shakedown-chat-window-openings-proof-v0-2026-05-02.md` cites first demand `.userdata/dev-harness/harness_runs/20260502_065253/` and reopened demand `.userdata/dev-harness/harness_runs/20260502_065445/`; `doc/bandit-extortion-playthrough-audit-harness-skill-packet-v0-2026-04-22.md` cites the older teachable standoff/playthrough surface.
- **Why not ready for this card:** no fresh bandit contrast row was found for the requested no-signal vs fire/smoke/basecamp signal vs high-threat/resistant matrix. More importantly, Josef's camp/NPC assignment preflight remains required for bandit/basecamp threat rows: unassigned current-save NPCs must not be credited as low/high camp threat evidence. Camp/NPC assignment was not audited in the inspected fresh pass.
- **Expected read when repaired:** no obvious signal should avoid instant theatrical pressure; fire/smoke/basecamp signal should pull scouting, shakedown, or hold-off pressure quickly enough to judge; high-threat/resistant camp/player setup should stalk/hold off/back out/escalate differently instead of replaying the same tax-office demand branch.
- **Taste questions when repaired:** does pressure feel scenic and earned; do Pay/Fight/Refuse read as a normal dialogue surface; does the camp feel socially threatened rather than mechanically taxed; does high threat change bandit posture visibly?
- **Next honest step:** run camp/NPC assignment preflight, repair/neutralize loose NPC footing, then run a small fresh bandit contrast matrix before promoting this row to ready.

### 8. `/visions/camp-locker` and `/horde/camp` — Camp/NPC assignment preflight remains open for threat rows

The fresh locker utility row is ready, but it is not camp-threat proof. For any bandit/basecamp threat or high-threat contrast claim, the current harness save still needs a camp/NPC assignment audit. Low-threat rows must neutralize loose/unassigned extras; high-threat rows must prove enough properly camp-assigned members tied to the intended camp/faction/overmap-special state. Until that audit exists, do not use random nearby NPCs as threat evidence. Bureaucracy, yes, but at least it is not pretending strangers in the parking lot are your militia.

## Quick Josef ask

Start with the four ready rows if you want a useful taste pass now:

1. Cannibal night pressure (`/horde/camp`).
2. Zombie rider open-field pressure (`/rider/stalker/raptor`).
3. Writhing stalker hit-fade (`/rider/stalker/raptor`).
4. Camp locker weather/service (`/visions/camp-locker`).

For each: play 2-5 minutes, point to one moment, and answer whether it reads optically, mechanically, and gnostically; whether it feels fair/fun/alive; and what felt fake, hollow, invisible, or annoying.

Do **not** judge bandit contrast, fresh flesh raptor, or stalker fire/light contrast from this card yet; those are visibly caveated above.
