# Harness Control Lookup

_Practical control notes for automation. Not a full CDDA controls manual; only the stuff we actually care about for harness authoring._

## Principles
- These are **pragmatic automation notes**, not authoritative game documentation.
- Context matters. The same key may do something slightly different depending on UI state, quest prompts, nearby NPCs, or branch-specific menu differences.
- Prefer **observables + logs** over blind faith. If possible, confirm the resulting state from `llm_intent.log`, screenshots, or another artifact.
- Beware raw keybind semantics: gameplay `t` is throw, so accidental hotkey mismatches can produce surreal results like trying to throw boxer shorts at a cow. Typed characters and raw keybinds are not always interchangeable for harness work.

## Startup / main menu

| Goal | Keys | Notes |
|---|---|---|
| New Game -> Play Now! (default path on current `master`) | `n`, `d` | Current harness Phase-0 uses this minimal sequence. Branch-specific variants may diverge later. |
| Ignore debug popup | `i` | Popup text says `I/i` to ignore in the future. Timing/focus still matters. |
| Pass one turn / let queued output resolve | `.` | Current `dev` keybindings map pause/pass-turn to `.` (also `5` / keypad 5). Use this for deterministic one-turn advancement in harness probes. |
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

### Debug-menu caution for Package 2
- The shorthand `}`, `p`, `p`, `b`, `A` is **not** the current camp-state seam on the McWilliams fixture.
- After selecting an NPC in the debug editor, `b` currently opens **bionics**, and `A` there is CBM install, not camp assignment.
- For visible post-restage state inspection, the useful current debug-editor path is `}`, `p`, `p`, `2`, `Enter` on McWilliams (Katharina-specific index), which exposes the header with attitude / mission / faction after the real dialogue-side camp assignment path above.

## Important caveats learned live

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
