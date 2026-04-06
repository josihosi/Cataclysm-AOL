# TESTING

Current validation policy and current evidence only.

This file is not a trophy wall.
Remove stale or completed fluff instead of stacking crossed-off test history forever.

## Validation policy

Use the **smallest evidence that honestly matches the change**.

- **Docs-only change** -> no compile
- **Small local code change** -> narrow build and the narrowest relevant test target
- **Broad or risky code change** -> broader rebuild and relevant filtered tests
- **Before a Josef handoff** or after suspicious stale-binary behavior -> rebuild the real targets and rerun the real smoke path

### Agent vs Josef

Schani should do agent-side playtesting first whenever the harness or direct in-game probing can answer the question.
Josef should be asked only for:
- product judgment
- tone/feel
- human-priority choices
- genuinely human-only interaction

Josef being unavailable is **not** a blocker by itself.
Josef self-testing is **not** a plan blocker and does not belong in the active success state as a gate.
If Josef-specific checks are useful, write them down as non-blocking notes so he can run them later from his own list.
If another good agent-side lane exists, keep moving there.
If several human-only judgments are likely soon, batch them instead of sending tiny separate asks.

### Anti-churn rule

Do not keep rerunning the same startup or test packet when it is no longer the missing evidence class.
If startup/load is already green, and the missing proof is live behavior, then the next probe must target live behavior.
If a target is merely waiting on Josef, do not keep revalidating it unless the code changed.

---

## Current relevant evidence

### Harness first slice — reusable live probe contracts
Latest relevant harness evidence for the current helper wave:
- newest narrow validation for the harness interaction-reliability slice on current dirty HEAD
  - `git diff --check`
  - `python3 -m py_compile tools/openclaw_harness/startup_harness.py`
  - `python3 - <<'PY' ... alias/retry smoke for run_peekaboo_interaction / peekaboo_press_sequence ... PY`
  - passed:
    - harness Peekaboo interactions now refocus the Cataclysm window and retry once after a command-level failure instead of treating a single flaky press/type miss as final truth
    - human-readable contract keys now normalize to the names Peekaboo actually accepts (`enter -> return`, `backspace -> delete`, `esc -> escape`), so packaged step contracts keep working without each scenario reinventing key-name trivia
    - printable-text buffering still stays intact around those normalized control keys
- narrow validation for the newest item/monster helper slice checkpointed in `57a0e4ae7d`
  - `python3 -m py_compile tools/openclaw_harness/startup_harness.py`
  - `python3 - <<'PY' ... debug_spawn_item / debug_spawn_monster / execute_probe_steps smoke ... PY`
  - passed:
    - `debug_spawn_item` now drives the current debug wish path `}`, `s`, `w`, uses the uilist filter popup (`/` + query), handles the amount prompt, and exits back out cleanly
    - `debug_spawn_monster` now drives the current debug wish path `}`, `s`, `m`, uses the uilist filter popup (`/` + query), supports friendly / hallucination / group-radius toggles, confirms the look-around target, and exits back out cleanly
    - `execute_probe_steps` now reports those helpers explicitly with `debug_menu_path=["}", "s", "w"]` / `debug_menu_path=["}", "s", "m"]` plus honest spawn-target metadata (`inventory` vs `look_around_confirm`)
- prior narrow validation for the first landed helper slice still stands
  - `python3 -m py_compile tools/openclaw_harness/startup_harness.py`
  - `python3 - <<'PY' ... debug_spawn_follower_npc / execute_probe_steps smoke ... PY`
  - passed: helper path expands to the expected debug-menu shortcut sequence `}`, `s`, `f`, and `execute_probe_steps` now reports `debug_menu_path=["}", "s", "f"]` plus `spawn_type=random_follower_npc`
- newest narrow validation for screenshot-capture honesty on current dirty HEAD
  - `git diff --check`
  - `python3 -m py_compile tools/openclaw_harness/startup_harness.py`
  - `python3 tools/openclaw_harness/startup_harness.py probe chat.nearby_npc_basic`
  - `python3 tools/openclaw_harness/startup_harness.py probe ambient.weird_item_reaction`
  - passed:
    - harness screenshots now use direct `peekaboo image` capture on a selected Cataclysm window instead of `peekaboo see`, so blocked probes and per-step captures stop failing on element-detection timeout
    - screen summaries now record `window_id` / `window_index` alongside the captured title/build metadata, so stale-binary audits point at the actual window that was captured
    - blocked chat report `.userdata/dev-harness/harness_runs/20260406_074721/probe.report.json` still returns `blocked_runtime_prereqs`, but now with a real captured screen instead of an empty timeout shell
    - ambient report `.userdata/dev-harness/harness_runs/20260406_074747/probe.report.json` captures startup / before / after plus every step screenshot successfully on the shipped fixture; its remaining `inconclusive_version_mismatch` verdict is now a genuine binary-freshness issue rather than screenshot-tool noise
- landed harness uplift beyond the original locker slice still stands
  - `debug_spawn_follower_npc` is now the first reusable scenario-setup helper, wired to the current debug-menu path `}`, `s`, `f`
  - profile loading merges `tools/openclaw_harness/profiles/master.json` into non-`master` profiles, so shared startup policy reaches `dev-harness`
  - probe contracts can script key/text steps and choose artifact logs instead of only “advance turns + grep debug.log”
  - printable gameplay keys go through `peekaboo type` instead of invalid `peekaboo press` usage
  - startup no longer treats the first `lastworld.json` flip as sufficient proof of ready gameplay; `post_lastworld_wait_seconds: 8.0` gates the packaged path
  - `chat.nearby_npc_basic` installs the captured `dev` profile snapshot before the save fixture, so `dev-harness` inherits the saved chat/keybinding state the probe expects
- current packaged chat blocker packet is still honest, but the blocker stack changed
  - narrow validation for the harness-side blocker demotion:
    - `python3 -m py_compile tools/openclaw_harness/startup_harness.py tools/openclaw_harness/log_probe.py`
    - `python3 -c "import tools.openclaw_harness.startup_harness as h; opts={'LLM_INTENT_BACKEND':'api','LLM_INTENT_PYTHON':'','LLM_INTENT_API_KEY_ENV':'CATA_API_KEY'}; print(h.resolve_game_runtime_python(opts)); print(h.probe_runtime_blockers('dev-harness','llm_intent.log')); print(h.probe_runtime_warnings('dev-harness','llm_intent.log'))"`
    - `python3 tools/openclaw_harness/startup_harness.py probe chat.nearby_npc_basic`
  - passed:
    - `.userdata/dev/config/options.json` and `.userdata/dev-harness/config/options.json` still both have `LLM_INTENT_PYTHON=''`, but that is no longer treated as a hard blocker on Josef’s Mac because the runtime resolves `/Users/josefhorvath/ollama/api_env311/bin/python3`
    - `/Users/josefhorvath/ollama/api_env311/bin/python3 -c 'import any_llm'` succeeds, so the old “empty option means no runnable Python” packet was overstated
    - the packaged chat report at `.userdata/dev-harness/harness_runs/20260406_085106/probe.report.json` now runs the scripted chat steps instead of skipping them with `blocked_runtime_prereqs`
    - the same report now carries runtime warnings instead of a fake hard blocker:
      - `llm_python_option_empty_using_runtime_fallback`
      - `llm_api_key_env_unset`
  - still blocked / inconclusive:
    - `CATA_API_KEY` is not present in the harness process environment
    - the latest run captured stale executable `6dcb5b91f7-dirty` while repo HEAD was `6dc4d9ed1e`, so the verdict is `inconclusive_version_mismatch`
    - that stale-window run produced no new `llm_intent.log` lines, so recipient/artifact proof is still missing on a genuinely current executable
- current ambient-scenario footing is still honest on the fresh current tiles binary
  - `python3 -m py_compile tools/openclaw_harness/startup_harness.py`
  - `python3 tools/openclaw_harness/startup_harness.py list-scenarios`
  - `make -j4 TILES=1 cataclysm-tiles`
  - `python3 tools/openclaw_harness/startup_harness.py probe ambient.weird_item_reaction`
  - `./zzip /tmp/.../#RGFuaWFsIEdvbnphbGV6.sav.zzip` + inspect `#RGFuaWFsIEdvbnphbGV6.sav` / `zones.json`
  - passed:
    - the fresh tiles build reports `6dcb5b91f7-dirty`, and the packaged ambient report at `.userdata/dev-harness/harness_runs/20260406_083006/probe.report.json` now captures a current-binary, version-matched run instead of hiding behind stale-binary noise
    - scenario listing still reports `ambient.weird_item_reaction` as `status: active` with no fake helper blocker
    - the packaged ambient contract still runs all three packaged steps (`debug_spawn_item` → `drop_item` → `advance_turns`) on the shipped fixture and returns to normal gameplay view after the scripted item/drop actions
    - the shipped `basecamp_dev_manual_2026-04-02` save still carries nearby follower/camp state (`followers: [ 2, 3 ]`; player-message history includes `Bruna Priest is assigned to Debug Central` and `Ricky Broughton is assigned to Debug Central`)
    - the latest ambient verdict is now the honest one: `inconclusive_no_artifact_match` with no fake helper blocker and no stale-binary excuse
- newest narrow validation for the helper/reporting slice that touched locker setup honesty
  - `python3 -m py_compile tools/openclaw_harness/startup_harness.py tools/openclaw_harness/log_probe.py`
  - `python3 tools/openclaw_harness/startup_harness.py list-scenarios`
  - `make -j4 TILES=1 cataclysm-tiles`
  - `python3 tools/openclaw_harness/startup_harness.py probe locker.weather_wait`
  - `./zzip /tmp/.../#RGFuaWFsIEdvbnphbGV6.sav.zzip` + inspect `#RGFuaWFsIEdvbnphbGV6.zones.json`
  - passed:
    - the harness now has a reusable `debug_force_temperature` helper on the current debug path `}`, `m`, `T`
    - probe contracts can now include a generic `wait` step, which was enough to show that the post-`lastworld.json` autoload "success" screenshot can still be on the verification splash while later step captures are in real gameplay
    - `locker.weather_wait` no longer fails because of stale binary or missing UI control; the real blocker is fixture shape
    - inspection of the shipped `basecamp_dev_manual_2026-04-02` save fixture shows camp/follower state but **no `CAMP_LOCKER` zone** in the fixture zone data, so the packaged locker contract cannot honestly emit `camp locker:` packets yet
    - that scenario is now supposed to stay explicitly blocked until a locker-capable fixture/restaging path exists
- meaning:
  - the missing evidence is no longer “does the packaged ambient contract even run?” or “can the harness even drive the locker temperature UI?”
  - the current blockers are: current-executable freshness plus API env for chat, missing ambient-reaction proof, and a missing locker-capable fixture/restaging path for `locker.weather_wait`
  - do not keep rerunning `chat.nearby_npc_basic` until the harness is pointed at a genuinely current game window again; once that is fixed, decide whether prompt-only recipient proof is enough or whether a real response packet is still needed
  - do not keep rerunning `locker.weather_wait` on the shipped fixture until the scenario has a real `CAMP_LOCKER` save shape again
  - do not keep claiming `ambient.weird_item_reaction` is blocked on assign-NPC helpers unless a new fixture/state actually proves that again

### Locker Zone V1 / V2 baseline + V3 deterministic slice
Latest relevant agent-side locker packet:
- broader locker baseline remains anchored by committed HEAD `1381284982`
  - `make -j4 tests`
  - `./tests/cata_test "[camp][locker]"`
  - passed at `171 assertions in 11 test cases`
- that broader suite still covers the bundled locker baseline from V1/V2 and the landed V3 temperature slices:
  - locker policy serialization / save-load
  - representative locker item classification
  - locker-zone candidate gathering and reservation filtering
  - locker loadout planning and disabled-category behavior
  - service / equip / upgrade / return behavior
  - one-worker-at-a-time queue sequencing plus dirty-trigger follow-through
  - transient assigned-camp downtime rehydration
  - `camp_locker_service_readies_ranged_loadouts_from_locker_supply`
    - managed ranged loadout starts empty
    - locker service counts the weapon’s inserted magazine as part of the two-magazine readiness budget
    - locker supply contributes only the additional compatible magazines needed to reach that cap
    - locker-zone ammo fills those selected magazines and reloads the supported weapon
    - leftover locker magazine count and ammo depletion stay deterministic
  - V3 deterministic outerwear coverage in `camp_locker_loadout_planning`
    - explicit cold-temperature probe (`35 F`) prefers a warmer outerwear upgrade (`jacket_light -> coat_winter`)
    - explicit hot-temperature probe (`85 F`) prefers a lighter outerwear upgrade (`coat_winter -> jacket_light`)
    - scope stays intentionally narrow: shirt/vest-slot outerwear that covers both torso and arms
  - V3 deterministic legwear coverage in `camp_locker_loadout_planning`
    - explicit cold-temperature probe (`35 F`) prefers full-length cargo pants over cargo shorts (`shorts_cargo -> pants_cargo`)
    - explicit hot-temperature probe (`85 F`) prefers cargo shorts over full-length cargo pants (`pants_cargo -> shorts_cargo`)
    - explicit duplicate-current hot probe keeps `pants_cargo` as the best current full-length legwear, marks `antarvasa` as duplicate current, and still selects `shorts_cargo` as the hot upgrade
    - scope stays intentionally narrow: pants-slot short-vs-full-length coverage only; it does **not** pretend to preserve layered lower-body nuance yet
- latest narrow validation for the new deterministic judgment lock on current HEAD `e6e2b38540-dirty`:
  - `make -j4 tests`
  - `./tests/cata_test "camp_locker_loadout_planning"`
  - passed at `68 assertions in 1 test case`
- note: `./tests/cata_test "[camp][locker]"` on `e6e2b38540-dirty` still reports `All tests passed (181 assertions in 11 test cases)`, but Catch exits non-zero because an existing `diag_value` debug error in the queue/downtime lane is treated as failure; the planner-only case above is the honest validation for this slice because no locker service code changed

Latest proportional runtime locker proof:
- outerwear lane on the recorded current-binary path
  - `make -j4 TILES=1 cataclysm-tiles`
  - controlled cold probe on `dev` / `Sandy Creek`
    - staged **Bruna Priest** in `jacket_light` with `coat_winter` on the `CAMP_LOCKER` tile via the repo `./zzip` save tool, then loaded through `python3 tools/openclaw_harness/startup_harness.py start --profile dev --world 'Sandy Creek'`
    - harness load passed cleanly on `.userdata/dev/harness_runs/20260405_204703`
    - current `debug.log` on `d3b61027ec-dirty` emitted the real locker service packet:
      - `camp locker: before Bruna Priest ... temp_f=45.0 ... vest=[light jacket ... <jacket_light>] ... locker_raw=[puffer jacket (poor fit)<coat_winter>]`
      - `camp locker: plan for Bruna Priest ... changes=[... vest upgrade light jacket ... <jacket_light> -> puffer jacket (poor fit)<coat_winter> ...]`
      - `camp locker: after Bruna Priest ... vest=[puffer jacket (poor fit)<coat_winter>] ... locker=[... vest=[light jacket<jacket_light>] ...]`
  - controlled hot probe on the same save shape
    - restaged **Bruna Priest** in `coat_winter` with `jacket_light` on the locker tile, loaded through the harness, then forced `85 F` in the live debug temperature control before advancing to the next service window
    - the resulting live current-binary packet showed the planner re-queueing a lighter-outerwear swap:
      - `camp locker: before Bruna Priest ... vest=[puffer jacket ... <coat_winter>] ... locker_raw=[light jacket (poor fit)<jacket_light>]`
      - `camp locker: queued Bruna Priest state-dirty ... plan=[vest upgrade puffer jacket ... <coat_winter> -> light jacket (poor fit)<jacket_light>]`
- legwear lane on the stale-binary-audited Ricky current-save path
  - 2026-04-05 stale-binary audit first corrected the evidence path on current HEAD: the running window title was still `6b6de8d477-dirty`, so the tiles target was rebuilt before collecting new runtime proof; the recorded live legwear packets below come from `d55ffe0a53`
  - restored the original current `dev` / `Sandy Creek` save, backed it up under `.userdata/dev/save/manual_probe_backups/20260405_232203_ricky_legwear_cold/`, and used the packed-save path that the save audit had already identified as authoritative
  - cold-side packet
    - staged **Ricky Broughton** in `shorts_cargo` inside packed overmap `overmaps/o.0.0.zzip`, left `pants_cargo` on the packed locker tile in `maps/3.0.0.zzip`, then loaded through `python3 tools/openclaw_harness/startup_harness.py start --profile dev --world 'Sandy Creek'`
    - a plain fresh autoload still was not the missing evidence class by itself, so the honest runtime step remained live post-load gameplay: focused the current game window and sent `peekaboo press tab --count 3 --app cataclysm-tiles --window-id 10427`
    - current `debug.log` on `d55ffe0a53` then emitted the real cold legwear locker packet at `23:23:32.719`:
      - `camp locker: action Ricky Broughton reason=worker-downtime ...`
      - `camp locker: reached worker_downtime for Ricky Broughton ...`
      - `camp locker: queued Ricky Broughton wake_dirty=true ...`
      - `camp locker: before Ricky Broughton ... pants=[cargo shorts > 6 items<shorts_cargo>] ... locker=[pants=[cargo pants (poor fit)<pants_cargo>]]`
      - `camp locker: plan for Ricky Broughton ... changes=[pants upgrade cargo shorts > 6 items<shorts_cargo> -> cargo pants (poor fit)<pants_cargo>] ...`
      - `camp locker: after Ricky Broughton ... pants=[cargo pants (poor fit)<pants_cargo>] ... locker=[pants=[cargo shorts<shorts_cargo>]]`
      - `camp locker: serviced Ricky Broughton applied=true ...`
  - hot-side packet
    - reused the same Ricky current-save path with `shorts_cargo` on the locker tile and Ricky wearing `cargo pants` alongside his existing `antarvasa`
    - current `debug.log` on `d55ffe0a53` then emitted the real hot legwear locker packet at `23:59:27.798`:
      - `camp locker: queued Ricky Broughton state-dirty ... plan=[pants upgrade cargo pants<pants_cargo> -> cargo shorts (poor fit)<shorts_cargo>; pants dedupe keep=cargo pants<pants_cargo> drop=[antarvasa > 6 items<antarvasa>]] ...`
      - `camp locker: before Ricky Broughton ... pants=[antarvasa > 6 items<antarvasa>; cargo pants<pants_cargo>] ... locker=[pants=[cargo shorts (poor fit)<shorts_cargo>]]`
      - `camp locker: after Ricky Broughton ... pants=[cargo shorts (poor fit)<shorts_cargo>] ... locker=[pants=[cargo pants<pants_cargo>; antarvasa<antarvasa>]]`
      - `camp locker: serviced Ricky Broughton applied=true ...`
  - restored the staged save from the manual backup after the captures so the default `dev` world did not stay pinned to the probe state

Meaning:
- Locker Zone V1/V2 remain preserved under the landed V3 code path at deterministic-suite level
- the outerwear and currently implemented legwear V3 lanes now both have deterministic proof and proportional runtime proof on recorded current-binary / current-save paths
- the `antarvasa` result is now an explicit policy judgment, not a mystery bug: current locker behavior is still one managed pants item per slot, so extra pants-slot garments are treated as duplicates and returned to the locker when a hot/cold swap lands
- the missing harness work is no longer “can we package/report a first slice at all?”; it is now stronger trigger/setup coverage and more scenarios

---

## Pending probes

### Active queue — hackathon runway: stabilization + harness

1. extend the current harness uplift slice from `doc/harness-first-slice-plan-2026-04-06.md`
   - keep the reusable live probe contract/profile/save path honest on the current binary
   - keep post-load readiness honest; the `lastworld.json` flip alone was not sufficient for the chat path
   - keep runtime blockers explicit so blocked probe classes fail fast instead of pretending they are behavior verdicts
   - avoid probe-method drift mid-claim
2. extend scenario-setup helpers beyond the currently landed follower/item/monster/drop slice
   - keep `debug_spawn_follower_npc` on the current `}`, `s`, `f` path honest if debug bindings drift
   - keep `debug_spawn_item` on the current `}`, `s`, `w` path honest if the wish filter / amount prompt behavior shifts
   - keep `debug_spawn_monster` on the current `}`, `s`, `m` path honest if the wish filter / target-confirm behavior shifts
   - keep `drop_item(...)` honest on `d` if inventory filtering or amount-prompt behavior shifts
   - only add assign-NPC helper(s) if a concrete alternate-restaging or stronger-probe need actually appears
3. replace or restage the locker probe save shape before using `locker.weather_wait` again
   - the shipped `basecamp_dev_manual_2026-04-02` fixture lacks `CAMP_LOCKER`, so the current contract is correctly parked as a fixture blocker rather than a behavior verdict
4. keep `chat.nearby_npc_basic` parked until the harness is pointed at a genuinely current executable again, then resume recipient / `llm_intent.log` proof
   - current blocker packet is `.userdata/dev-harness/harness_runs/20260406_085106/probe.report.json`
   - once the current-executable path is real again, rerun `chat.nearby_npc_basic` and decide whether prompt-only proof is enough or whether `CATA_API_KEY` must be present for a response packet
   - `ambient.weird_item_reaction` is already runnable on the shipped fixture; next missing evidence is a real reaction/artifact packet, not a fake helper-unblock ceremony
5. after the probe/helper footing is stronger, package a compact Josef-facing testing packet before the pre-holiday active-testing window gets chewed up by setup friction
5. do not spend more runs collecting fresh locker packets unless code changes or the harness work invalidates the recorded current-save path
6. keep per-NPC personality nuance and decorative side quests out unless the stabilization/harness work exposes a truly smaller corrective slice

### Non-blocking Josef notes

None yet for the hackathon runway lane.
If later needed, add notes here without turning them into blockers.

---

## Reusable commands

Use these when they are actually the missing evidence, not as ritual.

### Narrow deterministic Basecamp bark / routing check
- `./tests/cata_test "[camp][basecamp_ai]"`

### Narrow deterministic locker check
- `./tests/cata_test "[camp][locker]"`

### Fresh full test rebuild on this Mac
- `make -j4 tests`

### Fresh tiles relink on this Mac
- `make -j4 TILES=1 cataclysm-tiles`

### Startup/load smoke
- `python3 tools/openclaw_harness/startup_harness.py start --profile dev --world 'Sandy Creek'`

### Packaged live probes
- `python3 tools/openclaw_harness/startup_harness.py probe locker.weather_wait`
- `python3 tools/openclaw_harness/startup_harness.py probe chat.nearby_npc_basic`

## Local build caveat

On this Mac, treat top-level `make -j4 tests` as the reliable path for a fresh `cata_test`.
Avoid treating `make -C tests cata_test` as authoritative here; it has been a repeated source of toolchain/stale-build nonsense.
Also: if you actually need a fresh tiles binary, use `make -j4 TILES=1 cataclysm-tiles`; plain `make cataclysm-tiles` is not an honest rebuild path here.
