# NPC LLM command scope and native coverage

## Code-grounded catalog

The primary response parser is `llm_intent.cpp :: allowed_actions()` plus the two dynamic
field forms in `parse_csv_payload()`. A valid response has speech and at most three action
tokens (with one optional `move` field); this is parser acceptance, not gameplay proof.

| command / intent | parser status | native consumer and consequence | material premises |
|---|---|---|---|
| `wait_here` | accepted | `npcmove.cpp :: npc::execute_llm_intent_action`: clears hold flag, `assign_guard`, native pause | primary target is a live player ally; action must pass the safe/no-danger gate |
| `hold_position` | accepted | assigns guard, sets hold-position flag, native pause; later release when player distance exceeds 15 | ally, safe turn; exact position/guard ownership must be observable |
| `follow_close` | accepted | stops guard, sets close-follow rule/override, native follow action | ally; safe turn and no pending pickup deferral; mission/Patrol/danger can supersede it |
| `follow_far` | accepted | stops guard, clears close-follow rule/override, native follow action | same as follow-close |
| `equip_gun` | accepted | sets gun rule, selects/wields `evaluate_best_gun()` | ally; can apply in danger; a suitable gun is needed for wield consequence |
| `equip_melee` (the supported “melee” intent) | accepted | clears gun rule, selects/wields best melee, or unwields a gun if none | ally; can apply in danger; item availability changes concrete result |
| `equip_bow` | accepted | enables gun+silent rules, selects silent gun then best gun, wields if present | ally; can apply in danger; suitable weapon needed |
| `panic_on` | accepted | forces 20 panic turns, adds run-away effect, clears movement arrival/target | ally response dispatch; subsequent danger/AI ownership may affect movement |
| `panic_off` | accepted | clears forced panic, starts 30-turn calm window, removes run-away effect | ally response dispatch; resulting panic decay is turn-dependent |
| `look_around` | accepted, then removed from primary action queue | secondary `enqueue_look_around_request`; `parse_look_around_response`; `set_llm_intent_item_targets`; `npcmove.cpp :: apply_llm_intent_item_targets` then native pickup | ally; nearby visible items within radius 5, allowed item IDs, pickup eligibility, no danger/flee/NO_NPC_PICKUP; up to four selections |
| `look_inventory` | accepted, then removed from primary queue | secondary inventory request/selection; `apply_look_inventory_actions` performs wear, wield, activate, or drop and records action status | ally; current inventory IDs; `can_wear`/`can_wield` for those operations; activation/drop can still have item-specific outcomes |
| `idle` | accepted | no `llm_intent_action` (maps to `none`); leaves ordinary NPC AI to choose the turn | ally response only; it is a valid no-change command, not a native action receipt |
| `attack=<target>` | accepted dynamic field; target is lower-case alphanumeric/underscore handle | target stored on NPC transient state; `apply_llm_intent_target` resolves snapshot legend/name/symbol, then forced attack path selects native melee/ranged attack or advances toward target | ally; target must remain visible/resolvable and attackable; max 4 attacks/30 turns, panic and normal danger/mission ownership can block/supersede |
| `move=<dx>,<dy> <wait_here\|hold_position>` | accepted dynamic field; dx/dy constrained to -20..20 | resolves against snapshot origin, stores `goto_to_this_pos`; native pathing advances, then applies arrival wait/hold | ally; same z-level/pathable destination, safe movement gate, no competing owner; blocked path clears stale target |

`llm_intent_action` in `npc.h` contains only the nine queued controls plus `none`; it does
not contain `look_around`, `look_inventory`, `attack`, or `move`. Those four are separate
secondary/dynamic routes. `none` is an internal sentinel, not a prompt command. No old action
names are accepted by the current parser: tokens must be in `allowed_actions()` or the two
dynamic forms. The ambient prompt is speech-only and must not be counted as an action route.

All primary response dispatch is additionally guarded by `resp.ok`, parse success, NPC lookup,
and `target->is_player_ally()` (`llm_intent.cpp` response processing). Non-allied recipients
have queued intent cleared in `npcmove.cpp`; this is an executable eligibility boundary, not
parser rejection.

## Existing behavioral proof and boundary

`tests/llm_intent_test.cpp` proves prompt wording, CSV parsing/rejection, dynamic move parsing
and origin math, look-around selection filtering/cap, snapshot target legends, and that a
resolved neutral handle becomes the NPC current hostile target after `move()`. It also directly
exercises native pathing to a move target, unreachable-path cleanup, hold-position release, and
wait-here persistence when the player separates. The leading-separator test reaches a queued
`equip_melee` intent, but does not run the native wield consequence. These are unit/direct-state
checks; generation, parser acceptance, queue state, target selection, and setup receive no
localLLM-to-native proof credit.

The retained R-031 ledger material supplies source-bound native slices for one free-text follow
chain, stay/guard behavior, ambient recipient/reply, camp-craft, competing camp-guard ownership,
and separate continuity/persistence components. Those receipts are scoped to their named actors,
utterances, and ownership premises; they do not generalize to this complete command catalog.
See `.de67/FS.md` R-031-S001 and the R-031 entries in `.de67/work-ledger.md` for the explicit
requirements that request identity, runner result, applying turn, physical consequence, and
competing-owner context be correlated.

First missing native command boundaries are: each queued equipment command's actual inventory
selection/wield result (`equip_gun`, `equip_melee`, `equip_bow`); panic on/off's observable
effect and subsequent turn behavior; an end-to-end localLLM response for `look_around` through
selected item pickup; `look_inventory` through each wear/wield/activate/drop branch; `attack=`
through an attributable hit/engagement consequence for melee and ranged weapons; and `move=`
from the actual response through path progress and arrival state. Existing follow/stay receipts
cover only their tested premises and must not be used as blanket coverage for the other commands.

## Minimal FS testing slice

Use one fresh source-bound native scenario per family, with a deterministic localLLM response
and exact request/recipient correlation: (1) safe movement + wait/hold and both follow modes,
(2) each equipment mode with an appropriate carried weapon, (3) panic on then off across
applying turns, (4) look-around with visible ground/container/vehicle items and a native pickup
receipt, (5) inventory with separate wear, wield, activate, and drop items, and (6) attack
against a visible lettered hostile at melee range and a ranged-capable setup, plus one `move=`
arrival case. For every case inspect mission/Patrol/guard/danger/vehicle ownership before
attribution, bind runner calculation completion to the exact response and applying turn, then
record the native state/message/action-status consequence. Include one blocked-premise case for
target loss or danger only where it distinguishes an executable boundary. Do not count prompt,
parser, generated text, accepted input, prewarm, or UI/OCR as native evidence.

## Dual-route runner handoff (read-only source inspection)

The game selects API mode when `LLM_INTENT_BACKEND=api` or hidden
`LLM_INTENT_USE_API=true`; it passes `--api-provider`, `--api-model`, optional
`--api-key-env`, and `--max-tokens` to `tools/llm_runner/runner.py`. The visible
options schema in `src/options.cpp` defaults to provider `openai`, key-name
`CATA_API_KEY`, and model `gpt-4.1-mini`; `LLM_INTENT_API_MODEL` is free text, so
`gpt-5.6-luna` can be configured without a source change. The runner calls
`any_llm.completion(model=..., provider=..., messages=[{role:user,content:prompt}],
api_key=...)` and extracts returned completion text. There is no model allowlist,
Responses API call, reasoning parameter, or compatibility probe in this code. Thus
`gpt-5.6-luna` readiness is unverified until an actual API request succeeds and emits
the expected one-line NPC response; the code path is Chat Completions-shaped.

Safe credential contract: configure only the environment-variable *name* in
`LLM_INTENT_API_KEY_ENV`; never put a secret in options, logs, or a scenario file.
The runner reads that named variable. The harness may provision the child from the
named variable, the OpenAI fallback variable `OPENAI_API_KEY`, or its platform secure
store (macOS Keychain service label `Cataclysm-AOL LLM API`; Windows Credential
Manager). This inspection did not read any secret or enumerate the process
environment. A worker still needs a resolved Python executable importing `any_llm`:
`LLM_INTENT_PYTHON` may name it/its venv; on macOS API mode the source fallback is
`/Users/josefhorvath/ollama/api_env311`, otherwise `/usr/bin/python3` (Windows
platform default is `python`). The harness explicitly reports missing Python,
missing `any_llm`, missing key-name, or missing credential as setup failures.

For E4B, keep the same native case and response contract after a failed local route,
switch only backend/model/credential routing to the API child, and correlate the
same utterance, recipient, request ID, runner `llm_response_emitted`, applying turn,
and native consequence. A runner self-test/API completion proves transport/model
availability only; it cannot replace native application evidence. Concrete setup gaps
to resolve before commissioning are: confirm the worker's API Python has `any_llm`,
confirm the child receives a configured key name with a present credential without
exposing its value, and run one bounded API response using the selected model. The
source provides no evidence that `gpt-5.6-luna` is accepted by the installed
any-llm/OpenAI stack, so treat that as an explicit compatibility checkpoint. The API
mode's shutdown branch calls `ollama_unload` (a route-mismatched cleanup path); retain
emitted response evidence even if shutdown cleanup reports an Ollama-side error.
