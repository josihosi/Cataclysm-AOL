# Harness Control Lookup

_Practical control notes for automation. Not a full CDDA controls manual; only the stuff we actually care about for harness authoring._

## Principles
- These are **pragmatic automation notes**, not authoritative game documentation.
- Context matters. The same key may do something slightly different depending on UI state, quest prompts, nearby NPCs, or branch-specific menu differences.
- Keep evidence classes separate: startup/load, deterministic test, live/screen behavior, and artifact/log proof are not interchangeable. A probe that only loads a save and closes is `load proof only / inconclusive for feature`, even when auto-close and artifact capture worked correctly.
- Use the smallest direct evidence that decides the current claim. Screenshots and OCR are useful for visible UI state, but startup, step-ledger, log, saved-state, native-receipt, and gameplay evidence keep their own ceilings.
- A surprising or broken feature is claim-scoped. Preserve facts for unaffected claims, continue when their causal footing remains clean, and route an ordinary product defect through the playtest witness/debug queue rather than the capability-gap store.
- Beware raw keybind semantics: gameplay `t` is throw, so accidental hotkey mismatches can produce surreal results like trying to throw boxer shorts at a cow. Typed characters and raw keybinds are not always interchangeable for harness work.

## Registry-owned scenario requests

Use `scenario_registry_cli.py` for typed scenario requests, not this control lookup. Refresh declaration projection with `rebuild` and report bindings with `reconcile` when those owners need it, then submit `registry-query` with `requirements` and `preferences` predicates. Explain the returned hard rejection or selection from its evidence, lifecycle, freshness, and proof-route fields. An unselectable result is an inert draft with `executable: false`; stop there.

A coordinator playtest brief with its matching validated charter requests execution. The query stays non-launching; `registry-launch <selection-token>` supplies single-use technical authority and revalidates the token, source, route, and runtime before the canonical probe. Its finalizer ingests `probe.report.json` only after accepted cleanup. Report startup and feature verdicts separately, with cleanup. Use `registry-status` for lifecycle, verification, and history continuity; no history subcommand exists.

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
| Drop item from inventory | `d` | Harness helper path for `drop_item`. Current helper opens the normal drop inventory, then either selects a one-character inventory slot directly or uses the inventory filter (`/` + query) plus inventory `TOGGLE_ENTRY` (`l`) / optional `MARK_WITH_COUNT` (`!`) before `Enter`. Queries should match visible item text; one-character selectors are treated as raw inventory slots. Current caveat: exact fuel runs through `20260427_232220` fail the filtered multidrop primitive. The latest UI trace shows filter `plank` redraws to zero visible rows/no `typeid="2x4"` row before `TOGGLE_ENTRY`/`MARK_WITH_COUNT`; do not trust this path for fuel proof until the fixture/live inventory primitive is repaired. |
| Spawn item wish menu from debug path | `}`, `s`, `w` | Harness helper path for `debug_spawn_item`. Current helper drives the wish-menu text filter (`/` + query) and amount prompt, then exits the menu. Because the uilist filter matches displayed entry text, practical queries should be item names / visible text, not raw item ids. |
| Spawn monster wish menu from debug path | `}`, `s`, `m` | Harness helper path for `debug_spawn_monster`. Current helper drives the wish-menu text filter (`/` + query), optional friendly/hallucination toggles, then confirms the look-around target. Practical queries should be monster names / visible text, not raw monster ids. |
| Spawn random follower NPC from debug path | `}`, `s`, `f` | Landed harness helper path (`debug_spawn_follower_npc`). Spawns a random follower near the player with current debug-menu hotkeys. |
| Force temperature from debug path | `}`, `m`, `T`, `Down`, `Enter` | Landed harness helper path (`debug_force_temperature`). The current submenu lists `Reset` first and `Set` second, so the harness explicitly moves to `Set` before filling the numeric prompt. The shipped dev/dev-harness probe path currently assumes Fahrenheit. |
| Paint furniture with debug map editor | `}`, `m`, `M`, optional target movement, `r`, `/` + query, `Enter`, `Enter`, `Esc` | Landed harness helper path (`debug_map_editor_place_furniture`). Current green proof uses target `right` and query `f_chair`; the following save/writeback plus `audit_saved_map_tile_near_player` gate is the proof, not the keypath alone. |
| Paint field with debug map editor | `}`, `m`, `M`, optional target movement, `e`, `/` + query, `Enter`, `Enter`, `Enter`, `Esc` | Landed harness helper path (`debug_map_editor_place_field`). Current green proof uses target `right` and query `fd_smoke`; the extra `Enter` accepts the field-intensity menu before applying the brush. The following save/writeback plus `audit_saved_map_tile_near_player` gate is the proof, not the keypath alone. |
| Spawn Rubik from debug path | `}`, `s`, `p`, `O` | Current remembered path only; treat as provisional until reverified in automation. |

### Multidrop source/control lookup (2026-04-27)

| Question | Source-backed answer | Proof consequence |
|---|---|---|
| Filter semantics | `inventory_selector::query_set_filter()` stores the typed filter via `set_filter()`. `inventory_column::prepare_paging()` applies `inventory_selector_preset::get_filter()`, which delegates to `basic_item_filter(filter)` against item text, and reveals matching contained entries while a non-empty filter is active. | Filter text alone is not selection proof; the harness must trace the filtered Multidrop rows and show a visible/selectable `2x4`/plank row. |
| Active cursor / highlighted row | `inventory_column::prepare_paging()` resets invalid paging and calls `highlight(...)`; `inventory_selector::get_highlighted_position()` plus each column's highlighted row identifies the active cursor after redraw/input. | The drop proof must include row trace after filter and after input: active column, highlighted row, selectable state, type/name, location, available count, and chosen count. |
| Mark/count command | `inventory_multiselector` registers `MARK_WITH_COUNT` (`!`) and `TOGGLE_ENTRY` (`l`/Right/6). `MARK_WITH_COUNT` calls `query_count()` then `toggle_entries(query_result, SELECTED)`; `TOGGLE_ENTRY` calls `toggle_entries(count, SELECTED)`. `set_chosen_count()` clamps to available count and fills `to_use`; non-charge stacks add one `drop_location` per selected item. | A green count primitive requires `total_selected_qty=20` after `MARK_WITH_COUNT` / selected row `chosen_count=20`, not just the keypress. If the row is visible but selected quantity stays zero, classify `blocked_untrusted_drop_mark_count_primitive`. |
| Confirm / return-to-map condition | `inventory_drop_selector::execute()` only returns on `CONFIRM` when `to_use` is non-empty and stealing/liquid checks do not cancel; if `to_use` is empty it shows the no-items popup and remains in Multidrop. | A green exit primitive requires `inventory_drop_selector event=return title="Multidrop" action="CONFIRM" total_selected_qty=20` before any save key is sent. If selection is correct but no return appears, classify `blocked_untrusted_drop_menu_exit_primitive`. |

### Brazier and firestarter behavior

- `brazier` uses `deploy_furn` to create `f_brazier`; normal Apply opens the activatable-item selector and deployment asks for an adjacent tile.
- A charged lighter uses the normal firestarter target and activity path. `tests/firestarter_activity_test.cpp` owns deterministic ignition law; a live run still needs a native/UI result plus `f_brazier` and `fd_fire` state for the fire it claims.
- Inventory filtering, a keypress, fixture fuel, or a `SOURCE_FIREWOOD` zone is setup evidence, not deployment or ignition proof. The worker may use any competent normal-player route; preserve the resulting receipt, UI state, message, or saved-state fact instead of constitutionalizing one key sequence.

### Debug-menu caution for Package 2
- The shorthand `}`, `p`, `p`, `b`, `A` is **not** the current camp-state seam on the McWilliams fixture.
- After selecting an NPC in the debug editor, `b` currently opens **bionics**, and `A` there is CBM install, not camp assignment.
- For visible post-restage state inspection, the useful current debug-editor path is `}`, `p`, `p`, `2`, `Enter` on McWilliams (Katharina-specific index), which exposes the header with attitude / mission / faction after the real dialogue-side camp assignment path above.

## Important caveats learned live

### Time passage and interruptions

- `.` / `5` passes one turn; `|` opens the native wait activity. The cockpit also exposes primitive, guarded, and raw waiting with native receipts.
- Choose interruption handling for the experiment: stop, handle classified non-dangerous prompts, or permissively continue and receipt what was crossed. An interruption is a new observation, not automatically a failed run.
- Derive any horizon from the mechanic or observed progress. Record native time and transition facts, and do not treat a short uneventful interval as a lifecycle negative.

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
