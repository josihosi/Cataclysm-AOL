# Live bandit + Basecamp playtesting feasibility probe v0 (2026-04-21)

Status: checkpointed done for now; bounded feasibility packet exists.

## Greenlight verdict

Josef explicitly wants Andi to try starting the game and find out whether live playtesting is already practical with the new bandits plus the current Basecamp work in place.
The key uncertainty is not only feature logic, but setup reality: can the current tree be launched into a credible live probe path, and can overmap bandits be placed or induced on purpose without descending into debug-menu folklore.

This is a bounded feasibility probe, not a demand to prove the whole game is already playtest-perfect.

## Scope

1. Start the current game on a real live path and determine whether a credible playtesting setup for current bandits plus current Basecamp work can be reached.
2. Check whether current debug/harness/control paths can stand up the needed scenario footing, including whether overmap bandits can be spawned, placed, or otherwise made testable on purpose.
3. Produce one reviewer-readable packet that separates:
   - screen: what happened in the running game
   - tests: what deterministic checks already guarantee
   - artifacts/logs: what was or was not recorded
4. Classify the result honestly as something like feasible now, feasible with narrow restage/setup help, or blocked on a specific missing setup path.

## Non-goals

- claiming broad game readiness from one live packet
- replacing the already-closed audit or locker packets with live-theater improvisation
- fresh bandit architecture or Basecamp feature work
- hand-waving setup ambiguity away just because debug menus are large and weird
- turning one inconclusive live run into fake product truth

## Success shape

This item is good enough when:
1. one bounded live playtesting feasibility packet exists for current bandits plus current Basecamp footing
2. the packet says plainly whether live playtesting is already practical on the current tree
3. the packet says plainly whether overmap-bandit setup/spawn control is currently available, narrowly restageable, or still missing
4. the packet separates screen/tests/artifacts cleanly instead of flattening them into one soup verdict
5. any blocker is stated as a concrete missing setup/control path rather than vague "playtesting hard" whining
6. the slice stays bounded and does not turn into open-ended live playtesting theater

## Bounded probe contract

Evidence classes deliberately used:
- startup/load feasibility on a fresh current build
- setup/control-surface availability for an intentional bandit restage seam

This packet still does **not** pretend to prove broad game-readiness.
It answers the narrower question: can the current tree be relaunched cleanly and can a real bandit-plus-Basecamp live setup be restaged on purpose without inventing fresh code.

Commands and probes used:
- `python3 tools/openclaw_harness/startup_harness.py start --profile dev --world 'Sandy Creek'`
- `python3 tools/openclaw_harness/startup_harness.py list-scenarios --profile dev`
- `find tools/openclaw_harness -iname '*bandit*' -o -iname '*basecamp*'`
- `python3 tools/openclaw_harness/startup_harness.py probe bandit.named_spawn_probe_temp --profile dev-harness --world McWilliams --fixture mcwilliams_live_debug_2026-04-07 --replace-existing-worlds`

## Final bounded readout

### Screen

- A fresh current-build relaunch succeeded on the `dev` profile and `Sandy Creek` world, using character `Danial Gonzalez`.
- The success capture under `.userdata/dev/harness_runs/20260422_002122/` shows the live window title `Cataclysm: Dark Days Ahead - 5af2fb80d8-dirty`.
- A second bounded live probe on `dev-harness` / `McWilliams` reached the named-NPC spawn menu, filtered it to `bandit`, confirmed selection, and returned to live gameplay with a hostile spawned NPC on screen.
- The probe screen captures show the sequence plainly:
  - `open_named_npc_spawn_path.after.png` shows `Choose NPC:` with `3 bandit` visible
  - `filter_bandit_template.after.png` shows `3 bandit` highlighted plus bandit-family entries such as `bandit_trader`, `bandit_quartermaster`, and `bandit_mechanic`
  - `confirm_filtered_entry.after.png` and `post_spawn_settle.after.png` show live gameplay resumed with the message `Stefany Billings, Bandit gets angry!` and `Stefany Billings` visible in red

### Tests

- This packet still did **not** rerun the broad deterministic suites, because the missing truth here was live setup/control feasibility rather than whether the already-checkpointed deterministic packets were lying.
- Existing deterministic footing that still matters:
  - bandit mark/playback packets remain covered by the earlier closed bandit suites recorded in `TESTING.md`
  - the bounded Basecamp routing/readout surface remains covered by the `[camp][basecamp_ai]` tests cited in the audit packet
  - locker/service footing remains covered by the closed locker packet and its focused suite

### Artifacts / logs

Fresh current-build startup artifacts:
- run dir: `.userdata/dev/harness_runs/20260422_002122/`
- screen capture: `.userdata/dev/harness_runs/20260422_002122/success.png`
- capture metadata: `.userdata/dev/harness_runs/20260422_002122/success.peekaboo.json`
- captured repo/build head: `5af2fb80d8-dirty`
- `version_matches_repo_head = true`
- `version_matches_runtime_paths = true`

Bounded named-bandit probe artifacts:
- run dir: `.userdata/dev-harness/harness_runs/20260422_002329/`
- screen captures:
  - `.userdata/dev-harness/harness_runs/20260422_002329/open_named_npc_spawn_path.after.png`
  - `.userdata/dev-harness/harness_runs/20260422_002329/filter_bandit_template.after.png`
  - `.userdata/dev-harness/harness_runs/20260422_002329/confirm_filtered_entry.after.png`
  - `.userdata/dev-harness/harness_runs/20260422_002329/post_spawn_settle.after.png`
- the same probe captures were tied to the fresh current build with window title `Cataclysm: Dark Days Ahead - 5af2fb80d8-dirty`

Control-surface inventory result after the bounded probe:
- the harness tree still exposes several runnable **Basecamp** scenarios and fixtures on McWilliams footing
- the harness tree still does **not** expose a reviewer-clean packaged **bandit** scenario under `tools/openclaw_harness/scenarios`
- however, the current game plus current harness raw key/type grammar **does** expose one narrow intentional restage seam today: debug menu `}` -> spawn submenu `s` -> named NPC spawn `p`, then filter text `bandit` and confirm selection
- this seam was proved live on the current build, then the temporary scenario file used to drive the probe was removed again instead of being left behind as fake canon

## Final classification

Current honest classification after the fresh relaunch plus bounded live probe:

- **Basecamp live path:** reachable today on a fresh current build
- **combined bandit + Basecamp live playtesting:** narrowly practical today for bounded local restage, not yet reviewer-clean packaged as a one-command bandit harness scenario
- **overmap-bandit setup/spawn control:** narrowly restageable now through the existing named-NPC debug spawn seam, but still missing as a clean packaged overmap/harness scenario

This is no longer blocked on the two earlier uncertainties.
Those questions are now answered plainly:

1. **fresh current-build relaunch discipline is satisfied**
   - the fresh startup captures match repo head `5af2fb80d8-dirty`
2. **intentional bandit live setup/control footing exists, but it is narrow and ugly rather than packaged**
   - Basecamp scenarios already existed in the harness tree
   - bandit control did not surface as a packaged scenario, but the named-NPC debug path was proved live and intentional

## What this means

The honest state now is:
- you can relaunch into a fresh current-build live world
- you can do bounded Basecamp live probes on existing harness footing
- you can intentionally restage a hostile live bandit today without fresh code by using the named-NPC debug spawn path already reachable from current harness key/type control
- you still cannot honestly call the bandit side reviewer-clean or packaged, because that setup seam currently lives as a narrow debug/menu path rather than a first-class harness scenario

So the right verdict is not "missing" and not broad triumphalist nonsense either.
It is:
- **live playtesting is narrowly practical now on the current tree for bounded bandit + Basecamp restage work**
- **the remaining gap is packaging/ergonomics, not absence of a usable seam**

## Validation expectations

- choose the evidence class deliberately: startup/load, live behavior, and artifacts are different claims
- use the smallest reliable layer first, then go live only where the live question is real
- prefer fresh launches for current-build proof instead of leaning on stale open sessions
- if a live probe is inconclusive, say why it is inconclusive instead of laundering it into success/failure

## Position in canon

This packet is now checkpointed done for now.
It answered the active feasibility questions honestly:
- fresh current-build live relaunch is real
- bandit restage control is real but narrow
- a reviewer-clean packaged bandit harness scenario is still absent

If this lane reopens later, the next honest step is not another feasibility lap.
It is a separate packaging/helper question about whether the named-bandit restage seam should become a first-class harness scenario.
