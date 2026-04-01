# Harness Control Lookup

_Practical control notes for automation. Not a full CDDA controls manual; only the stuff we actually care about for harness authoring._

## Principles
- These are **pragmatic automation notes**, not authoritative game documentation.
- Context matters. The same key may do something slightly different depending on UI state, quest prompts, nearby NPCs, or branch-specific menu differences.
- Prefer **observables + logs** over blind faith. If possible, confirm the resulting state from `llm_intent.log`, screenshots, or another artifact.

## Startup / main menu

| Goal | Keys | Notes |
|---|---|---|
| New Game -> Play Now! (default path on current `master`) | `n`, `d` | Current harness Phase-0 uses this minimal sequence. Branch-specific variants may diverge later. |
| Ignore debug popup | `i` | Popup text says `I/i` to ignore in the future. Timing/focus still matters. |
| Pass turns / let queued output resolve | `Tab` | Usually one or two presses are enough for queued NPC answer injection after `C+b`. |

## In-game interaction probes

| Goal | Keys | Notes |
|---|---|---|
| Talk to nearby NPC | `C`, `t` | First practical target in the current save is Ricky Broughton. |
| Open freeform player utterance | `C`, `b` | Type utterance, then `Enter`, then usually `Tab` x1-2 to let the response inject. |
| Open ruleset window from chat UI | `a` (sometimes `a`, then `a` again) | Quest-first chat variants may consume the first `a`; if so, press `a` again to reach the ruleset window. |
| Spawn Rubik from debug path | `}`, `s`, `p`, `O` | Current remembered path only; treat as provisional until reverified in automation. |

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

## Suggested harness posture
- use this lookup table as a **starting point**
- keep per-branch/profile overrides in config
- write down deviations when discovered instead of trusting memory
