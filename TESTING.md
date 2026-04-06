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
- prior harness honesty validation for the chat blocker packet still stands
  - `python3 -m py_compile tools/openclaw_harness/startup_harness.py`
  - `python3 tools/openclaw_harness/startup_harness.py probe chat.nearby_npc_basic`
- landed harness uplift beyond the original locker slice still stands
  - `debug_spawn_follower_npc` is now the first reusable scenario-setup helper, wired to the current debug-menu path `}`, `s`, `f`
  - profile loading merges `tools/openclaw_harness/profiles/master.json` into non-`master` profiles, so shared startup policy reaches `dev-harness`
  - probe contracts can script key/text steps and choose artifact logs instead of only “advance turns + grep debug.log”
  - printable gameplay keys go through `peekaboo type` instead of invalid `peekaboo press` usage
  - startup no longer treats the first `lastworld.json` flip as sufficient proof of ready gameplay; `post_lastworld_wait_seconds: 8.0` gates the packaged path
  - `chat.nearby_npc_basic` installs the captured `dev` profile snapshot before the save fixture, so `dev-harness` inherits the saved chat/keybinding state the probe expects
- current-binary chat evidence is now honest instead of stale-binary theater
  - before the harness patch, `python3 tools/openclaw_harness/startup_harness.py probe chat.nearby_npc_basic` on current tiles binary `3867b1c930` reached startup successfully but still ended at `verdict: inconclusive_no_new_artifacts`; report: `.userdata/dev-harness/harness_runs/20260406_022006/probe.report.json`
  - direct option/runtime audit after that run showed the real blocker class, not a mysterious chat regression:
    - `.userdata/dev/config/options.json` and `.userdata/dev-harness/config/options.json` both have `LLM_INTENT_PYTHON=''`
    - `CATA_API_KEY` is not present in the harness process environment
    - local stock `python3` cannot satisfy the runner anyway (`python3 tools/llm_runner/runner.py --self-test --backend api ...` -> `any-llm import failed: No module named 'any_llm'`)
- newest packaged probe now reports that blocker explicitly
  - `python3 tools/openclaw_harness/startup_harness.py probe chat.nearby_npc_basic`
  - packaged report at `.userdata/dev-harness/harness_runs/20260406_022634/probe.report.json`
  - startup reaches gameplay on current tiles binary `3867b1c930`
  - probe steps are skipped with `status: skipped_runtime_blocker`
  - verdict is now `blocked_runtime_prereqs` with explicit blockers:
    - `llm_python_missing` (`LLM_INTENT_PYTHON` empty)
    - `llm_api_key_env_unset` (`CATA_API_KEY` absent)
- newest ambient-scenario audit: the shipped fixture already carries the camp/follower state, so the packaged probe runs today instead of helper-blocked folklore
  - `python3 tools/openclaw_harness/startup_harness.py list-scenarios`
  - `python3 tools/openclaw_harness/startup_harness.py probe ambient.weird_item_reaction`
  - `zstdcat 'tools/openclaw_harness/fixtures/saves/dev/basecamp_dev_manual_2026-04-02/save/Sandy Creek/#RGFuaWFsIEdvbnphbGV6.sav.zzip' | rg -n '"followers"|assigned to Debug Central'`
  - passed:
    - scenario listing now reports `ambient.weird_item_reaction` as `status: active` with no fake helper blocker
    - packaged probe report at `.userdata/dev-harness/harness_runs/20260406_063019/probe.report.json` runs all three packaged steps (`debug_spawn_item` → `drop_item` → `advance_turns`) on the shipped fixture
    - the shipped `basecamp_dev_manual_2026-04-02` save already carries `followers: [ 2, 3 ]`
    - the same save’s player-message history already records `Bruna Priest is assigned to Debug Central` and `Ricky Broughton is assigned to Debug Central`
    - the latest packaged verdict is `inconclusive_version_mismatch` on stale tiles binary `3867b1c930`, with no artifact matches; that is an evidence/freshness problem, not `blocked_helper_gap`
- newest narrow validation for the first non-debug inventory helper slice:
  - `python3 -m py_compile tools/openclaw_harness/startup_harness.py`
  - `python3 - <<'PY' ... drop_item / execute_probe_steps smoke ... PY`
  - `python3 tools/openclaw_harness/startup_harness.py list-scenarios`
  - passed:
    - `drop_item(...)` now drives the normal inventory drop action `d`, choosing either a raw one-character slot or a filtered visible-text query
    - count handling is wired through the amount prompt, and the helper exits back to gameplay cleanly in the harness contract path
    - `execute_probe_steps` now reports `drop_item` steps explicitly with `inventory_action=drop_item` and `selection_mode=slot|filter`
    - that helper slice was enough to make the packaged ambient contract executable on the shipped fixture; the remaining gap is reaction/artifact proof, not missing setup ritual
- meaning:
  - the missing evidence is no longer “does the packaged ambient contract even run?”
  - the current blocker is still local runner configuration for chat, plus missing ambient-reaction proof / stale-binary freshness for the ambient lane
  - do not keep rerunning `chat.nearby_npc_basic` until a real runner path/config exists; use the harness lane on unblocked proof/setup work meanwhile
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
3. strengthen `locker.weather_wait` so it reaches a real locker-trigger packet instead of only load/wait/tileset-noise
4. keep `chat.nearby_npc_basic` parked until runner prerequisites exist, then resume recipient / `llm_intent.log` proof
   - current blocker packet is `.userdata/dev-harness/harness_runs/20260406_022634/probe.report.json`
   - once the runner path/config is real again, rerun `chat.nearby_npc_basic`
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
