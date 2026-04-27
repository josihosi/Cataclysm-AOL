# Harness Control Lookup

_Practical control notes for automation. Not a full CDDA controls manual; only the stuff we actually care about for harness authoring._

## Principles
- These are **pragmatic automation notes**, not authoritative game documentation.
- Context matters. The same key may do something slightly different depending on UI state, quest prompts, nearby NPCs, or branch-specific menu differences.
- Keep evidence classes separate: startup/load, deterministic test, live/screen behavior, and artifact/log proof are not interchangeable. A probe that only loads a save and closes is `load proof only / inconclusive for feature`, even when auto-close and artifact capture worked correctly.
- For GUI/live feature probes, every screenshot checkpoint should name the expected visible fact it is supposed to prove. If the screenshot only shows “the game loaded”, do not reuse it as proof that Smart Zones, bandit standoff, fire setup, or another feature worked.
- Probe/handoff reports now write `<mode>.step_ledger.json` plus embedded `step_ledger_summary`. `artifacts_matched` is feature proof only when startup is clean, every scenario step ledger row is green, and wait ledgers are not yellow/blocked. Use `expected_visible_fact` plus `expected_screen_text_contains` / `expected_screen_text_after_contains` on capture checkpoints when OCR text is the guard.
- Prefer **observables + logs** over blind faith. If possible, confirm the resulting state from `llm_intent.log`, screenshots, or another artifact.
- Beware raw keybind semantics: gameplay `t` is throw, so accidental hotkey mismatches can produce surreal results like trying to throw boxer shorts at a cow. Typed characters and raw keybinds are not always interchangeable for harness work.

## Startup / main menu

| Goal | Keys | Notes |
|---|---|---|
| New Game -> Play Now! (default path on current `master`) | `n`, `d` | Current harness Phase-0 uses this minimal sequence. Branch-specific variants may diverge later. |
| Ignore debug popup | `i` | Popup text says `I/i` to ignore in the future. Timing/focus still matters. |
| Pass one turn / let queued output resolve | `.` | Current `dev` keybindings map pause/pass-turn to `.` (also `5` / keypad 5). Use this for deterministic one-turn advancement in harness probes. |
| Wait for several minutes | <kbd>&#124;</kbd> (`Shift+\`) then menu choice | Current `dev` keybindings map action id `wait` / `ACTION_WAIT` to <kbd>&#124;</kbd>. The menu currently offers `1`=20s, `2`=1m, `3`=5m, `4`=30m, and with a watch `5`=1h, `6`=2h, `7`=3h, `8`=6h, plus daylight/noon/night/midnight/weather options. Use this for long time-passage probes instead of hundreds of `.` turns, but only after proving interruption/prompt handling for the scenario. |
| Let queued NPC answer injection resolve after `C+b` | `.` x1-2 | The current practical path is to burn one or two real turns, not `Tab`; on this branch `Tab` opens the main menu and sabotages live probes. |

## In-game interaction probes

| Goal | Keys | Notes |
|---|---|---|
| Talk to nearby NPC | `C`, `t` | First practical target in the current save is Ricky Broughton. |
| Assign nearby NPC to camp (current McWilliams Katharina restage) | `C`, `t`, `1`, `b`, `d`, `n`, `a`, `q`, `c` | Current Package 2 restaging helper path. This is the real nearby-hearer camp-assignment seam on the McWilliams fixture today. `1` is branch/save-order specific. |
| Open freeform player utterance | `C`, `b` | Type utterance, then `Enter`, then usually `Tab` x1-2 to let the response inject. |
| Open ruleset window from chat UI | `a` (sometimes `a`, then `a` again) | Quest-first chat variants may consume the first `a`; if so, press `a` again to reach the ruleset window. |
| Drop item from inventory | `d` | Harness helper path for `drop_item`. Current helper opens the normal drop inventory, then either selects a one-character inventory slot directly or uses the inventory filter (`/` + query), confirms the amount prompt, and exits back to gameplay. Queries should match visible item text; one-character selectors are treated as raw inventory slots. |
| Spawn item wish menu from debug path | `}`, `s`, `w` | Harness helper path for `debug_spawn_item`. Current helper drives the wish-menu text filter (`/` + query) and amount prompt, then exits the menu. Because the uilist filter matches displayed entry text, practical queries should be item names / visible text, not raw item ids. |
| Spawn monster wish menu from debug path | `}`, `s`, `m` | Harness helper path for `debug_spawn_monster`. Current helper drives the wish-menu text filter (`/` + query), optional friendly/hallucination toggles, then confirms the look-around target. Practical queries should be monster names / visible text, not raw monster ids. |
| Spawn random follower NPC from debug path | `}`, `s`, `f` | Landed harness helper path (`debug_spawn_follower_npc`). Spawns a random follower near the player with current debug-menu hotkeys. |
| Force temperature from debug path | `}`, `m`, `T`, `Down`, `Enter` | Landed harness helper path (`debug_force_temperature`). The current submenu lists `Reset` first and `Set` second, so the harness explicitly moves to `Set` before filling the numeric prompt. The shipped dev/dev-harness probe path currently assumes Fahrenheit. |
| Spawn Rubik from debug path | `}`, `s`, `p`, `O` | Current remembered path only; treat as provisional until reverified in automation. |

### Brazier deploy source/control lookup (2026-04-27)

This is a harness-primitive blocker, not a real-fire product failure. The normal player-action deploy primitive remains untrusted until a live confirmation proves the deploy prompt/selector state and saved target tile.

Source-backed facts:
- `data/json/items/tool/deployable.json` gives `brazier` `use_action: { "type": "deploy_furn", "furn_type": "f_brazier" }`.
- `game_menus::inv::use()` opens the `Use item` inventory via `activatable_inventory_preset`; activatable items include tool use methods such as the brazier.
- `inventory_pick_selector::execute()` returns the highlighted selectable item on `CONFIRM`, or an item selected by invlet/`SELECT`; filtering alone is not proof that the intended entry was returned.
- `deploy_furn_actor` calls `choose_adjacent( "Deploy where?" )` when deploying at the player position.
- `choose_adjacent` uses `choose_direction`, which registers `RIGHT`; default keybindings include right arrow and `l` for `RIGHT`. So `right` is valid only after the harness has really entered the `Deploy where?` direction prompt.

Current blocker label: `blocked_untrusted_brazier_deploy_selector`. Run `.userdata/dev-harness/harness_runs/20260427_200100/` proves the fixture inventory exists (`brazier=1`, `2x4=20`, `lighter=1`) but the saved east tile remains empty/missing `f_brazier`; run `.userdata/dev-harness/harness_runs/20260427_200919/` adds checked GUI-as-text guards and blocks at the first `Use item` menu proof. Do not try more blind key variants. Next acceptable proof needs stronger menu-entry/hotkey/GUI-text metadata for the inventory selector/deploy prompt before another live confirmation.

### Debug-menu caution for Package 2
- The shorthand `}`, `p`, `p`, `b`, `A` is **not** the current camp-state seam on the McWilliams fixture.
- After selecting an NPC in the debug editor, `b` currently opens **bionics**, and `A` there is CBM install, not camp assignment.
- For visible post-restage state inspection, the useful current debug-editor path is `}`, `p`, `p`, `2`, `Enter` on McWilliams (Katharina-specific index), which exposes the header with attitude / mission / faction after the real dialogue-side camp assignment path above.

## Important caveats learned live

### Long wait is a real primitive, not accelerated dot spam
- `.` / `5` is one-turn pause. It is appropriate for tiny deterministic turn burns and queued UI resolution.
- `|` is the real long-wait action. It creates an `ACT_WAIT` activity through the wait menu, and should be preferred for minutes/hours of live-world time passage once the scenario has verified the menu path.
- Treat any interruption as evidence, not noise: safe mode, hostile sightings, noises, hunger/thirst/sleep, damage, activity cancellation prompts, and wrong-screen focus can all invalidate a long-wait proof.
- Do not type `I`, `N`, `Y`, or `Esc` through prompts blindly. Capture the prompt/screen, classify it, and only ignore/cancel when the proof contract says that is the expected branch.
- A long-wait proof should record before/after game clock or turn, expected elapsed duration, whether the wait completed or interrupted, the typed choice, and the artifact/screenshot path. If elapsed time or prompt handling is missing, the verdict is not green.
- Harness scenario primitive: use `{"kind":"wait_action","choice_key":"3"}` for a 5m proof or `choice_key":"4"` for 30m. It captures `<label>.before`, `<label>.initial_wait_menu` when an alarm/watch pre-menu appears, `<label>.wait_menu`, and `<label>.after` screenshots/OCR, records expected duration plus prompt classification, and writes `<label>.before_wait.artifacts.log` / `<label>.after_wait.artifacts.log` deltas when artifact patterns are configured. It also writes a `wait_step_ledger` and top-level `wait_step_summary`; artifact matches no longer turn a wait probe green when the wait ledger is yellow/blocked. It does not type through interruption prompts.

### `C+b` recipient selection is situational
A live probe in the current `master` / `Sandy Creek` save showed:
- `C+b` utterance entry worked
- the response injected after `Tab`
- but the actual recipient was **Ricky Broughton**, not Rubik

So do **not** assume:
- “ambient NPC always overrides follower NPC”

Instead assume:
- recipient selection depends on current local state and should be confirmed from artifacts/logs

## Current known good observables
- `llm_intent.log` for who actually got the prompt/response
- startup harness run artifacts under `.userdata/<profile>/harness_runs/...`
- screenshots where UI state matters more than logs
- for live in-game probes, `peekaboo see` is currently more useful than plain `peekaboo image`; the raw `see` command may time out on element detection, but it still leaves behind a readable screenshot path in its debug logs

## Bandit extortion audit probes

Current named tiered probes for `Bandit extortion playthrough audit + harness-skill packet v0`:
- `python3 tools/openclaw_harness/startup_harness.py probe bandit.extortion_at_camp_standoff_mcw` — controlled-site stand-off setup / local-gate proof, not the shakedown menu.
- `python3 tools/openclaw_harness/startup_harness.py handoff bandit.extortion_at_camp_standoff_mcw` — leaves the live stand-off session running for manual review.
- `python3 tools/openclaw_harness/startup_harness.py probe bandit.extortion_first_demand_fight_mcw` — first Basecamp demand, `pay` / `fight`, then fight-forward message.
- `python3 tools/openclaw_harness/startup_harness.py probe bandit.extortion_first_demand_pay_mcw` — first Basecamp demand, pay branch, saved-world writeback inspection.
- `python3 tools/openclaw_harness/startup_harness.py probe bandit.extortion_reopened_demand_mcw` — controlled defender-loss reopen tier; proves the raised second demand still has `pay` / `fight`.

Keep the evidence classes split: screen/OCR for the visible menu, `probe.artifacts.log` for `shakedown_surface` fields, saved-world inspection for branch writeback, and deterministic `./tests/cata_test "[bandit][live_world][shakedown]"` for contract law.

## Bandit local sight-avoid / scout-return probes

Current named probe for `Bandit local sight-avoid + scout return cadence packet v0`:
- `python3 tools/openclaw_harness/startup_harness.py probe bandit.local_scout_return_preaged_mcw` — real nearby-owned-site local-contact footing plus the narrow `bandit_active_sortie_clock` fixture transform; proves the current runtime return-home decision and local-gate/shakedown skip while returning. Evidence class: return-home decision proof, not full walked-home/writeback proof and not live sight-avoid reposition proof.

## Practical live-probe recipe (current best cheap method)
1. focus the game window
2. send the command / utterance
3. pass turns with `Tab`
4. immediately read:
   - last ~40 lines of `llm_intent.log` if new lines appeared
   - and the visible in-game message log from a fresh screenshot if the intent log stayed quiet or looked ambiguous

This is crude, but much less error-prone than grepping the whole log and inventing a narrative afterward.

## Empirical probe notes from the current `Sandy Creek` field setup
- Probe: `Ricky, kill the cow.`
  - on-screen result: Ricky engaged the cow; log/screenshot evidence showed attack attempts and misses, plus waiting turns
  - no visible outright refusal
  - cow still alive after the sampled turns
- Probe: `Ricky, pick up the gun and axe on the ground.`
  - on-screen result: Ricky eventually picked up the bullpup shotgun
  - axe did not clearly show as picked up in the visible sampled log
  - socks still appeared present on the ground
  - there may be an initial refusal/delay before pickup behavior actually starts

## Suggested harness posture
- use this lookup table as a **starting point**
- keep per-branch/profile overrides in config
- write down deviations when discovered instead of trusting memory
