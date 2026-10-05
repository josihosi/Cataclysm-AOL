# Bandit visits: encounter and AI contract

Status: owner-approved target behavior, not a claim of completed implementation. This is the detailed FIRST-SMOKE encounter specification referenced by `.de67/FS.md`. R066 acceptance is historical; current scoped payment/return source owner is R067, with R051 retaining native input ownership. Existing accepted source and native results keep their original limits.

## Intent and individual survival

A shakedown party intends to obtain payment through a threat. Ordinary faction dislike or seeing the player/camp NPC must not turn that visit into combat before a demand. This intent applies through approach and simulation handovers; simulation ownership does not determine social intent.

Individual NPCs still respond to fire, zombies, other creatures, wounds, fear and forced incapacity. Fighting a third party does not mean declaring war on the player or camp. An actual attack by the player or a player-allied defender, including an avoided attack, can release combat against that side. Explicit Fight does so too. Preserve existing retreat and paid-departure behavior; no universal faction neutrality, invulnerability or unconditional wake rule.

## Contact and response

| Situation | Required behavior |
|---|---|
| Player outside/nearby | Establish real communication and issue the demand early; no exact doorstep requirement. |
| Player inside | An actually audible shout can establish contact without requiring the player to exit. |
| Camp NPC contacted | Any actual camp member can receive the threat; no leader recognition. The player can answer through a camp-response menu even when the avatar is elsewhere. |
| No receiver sees/hears the threat | No demand established and no refusal inferred. Continue existing bounded local search/withdrawal. |
| Empty locally simulated camp | Same search/withdrawal behavior; arrival is not contact. |
| Camp outside local reality-bubble simulation | Preserve the exact pending visit, awaiting local simulation. No offscreen demand, payment or auto-battle. Unloaded time does not count as refusal or failed local search. |
| Pending camp becomes locally simulated | Resume once from persisted state; establish actual contact before showing the response. No redispatch or teleportation. |

Reuse existing hearing/sound, sight and encounter rules. Do not invent a fixed range, infer exact avatar coordinates from camp knowledge, or fake an audible event. Remote UI represents control of the camp response, not remote hearing or offscreen simulation. If no locally simulated camp member can be contacted, remote-avatar presence alone cannot open it.

## Payment, combat and completion

Use the existing demand and Pay/Fight interaction, identifying the camp and receiving NPC where applicable. Remote payment uses physically reachable encounter/camp goods under existing ownership/value rules, never a distant avatar inventory. Preserve existing wealth-dependent prices. On successful shakedown payment, move the actual selected goods directly into the bound bandit home-camp stash as persistent, recoverable items, regardless of the collectors’ carrying capacity. This standard owner-selected destination replaces trader-inventory/current-tile custody for paid goods; do not convert them into points or leave a duplicate NPC copy. Preserve quantities, charges, contents, local payer eligibility and native prices. Commit payer debit, home-stash deposit and settlement coherently exactly once; cancellation/refusal leaves goods unchanged, failed destination placement must not lose goods or falsely succeed, and retries/reload cannot duplicate payment. Item teleportation is authorized; the NPCs still depart and travel home normally. For a roof/elevated encounter, a real reachable local descent may connect to the ground macro return route. Validate and physically execute that connection through existing navigation/ownership; neither changing only the route start z nor skipping the paid-return preflight is valid. Missing or blocked descent must remain an honest recoverable refusal. The scoped FIRST-SMOKE roof-payment amendment retains the native/source attribution limits and required goods-transfer/home-return proof.

Fight acts at the actual encounter; no actor is teleported. On a valid explicit Fight response, the actual physical receiver attempts ordinary `Character::shout("No, fuck you!")` once when that response is consumed. Use native voice, hearing and vocalization constraints; inability to vocalize does not prevent Fight. Preserve explicit-choice provenance separately from shared backout/failed-payment Fight handling: Pay, trade cancellation, backout, absence, duplicate response and reload cause no additional refusal shout. Do not substitute a distant avatar for an unavailable receiver, guarantee audibility or a target, or change pursuit/perception policy. Before a valid response, absence is not refusal. Actual attacks and survival reactions can interrupt contact legitimately. A third-party interruption alone must not convert the camp relationship into combat. Preserve existing finite search, withdrawal and return machinery where contact fails locally; waiting for local simulation is distinct from searching an empty loaded camp.

## State and handovers

Reuse existing operation/reservation, relationship and pending-interaction machinery. Maintain exact camp, operation/generation, member and receiver identity across abstract/local handover, unloading and save/reload. Record whether a valid communication/contact occurred and whether a response has resolved; prevent duplicate menus, stale responses and duplicate reservations. Real casualties, actual combat release and valid cancellation remain authoritative while a visit is pending. Do not reset hostility from a genuine prior attack merely to satisfy a test.

This contract does not prescribe a new state-machine framework. The implementation should use the smallest existing representation that can express these distinctions. It does not authorize auto-battle, autonomous camp payment, new wealth rules, cannibal diplomacy or unrelated faction changes.

## Production evidence and acceptance

| Sequence | Meaningful proof |
|---|---|
| Neutral approach → first sight → contact | Old production path escalates; corrected relationship/movement path retains peaceful intent and reaches a real demand. Include abstract/local handover. |
| Outdoor player / indoor audible shout | Existing contact/menu path works before exact doorstep; blocked/unheard counterpart does not invent contact. |
| Camp NPC / remote avatar | Correct local recipient and camp menu; payment conserves camp-local goods and leaves distant inventory unchanged. Include adjacent OMT contact where actual communication permits. |
| Zombie/fire interruption → resume | Native survival remains effective; third-party combat alone does not release anti-player hostility. |
| Avatar/allied attack or Fight | Actual combat release remains effective before/after handover, including avoided hits; flight/incapacity still applies. |
| Overmap wait → unload/reload → local activation | Same operation resumes once, without offscreen battle, duplicated dispatch or absence-as-refusal. |
| Empty camp, nonmember/stale identity, raid, paid return | No fabricated contact; exclusions and existing unrelated behavior retained. |
| Pending/resolved menu → reload/retry | Exact response and goods applied once; stale menu cannot affect another operation. |

Use existing production callers and opt-in traces. Source tests precede the affected Luna native branches on an immutable build, starting from the nearest compatible saves rather than replaying scouting. Automated test counts alone do not establish a native demand/payment/Fight outcome. Preserve8820/8915/8945/9240 and accepted R064 frontier proof.

## Current evidence and implementation boundary

R065 records first KILL at5286366 while approaching under abstract owner, avatar visible, no attack callback or harm. The current relationship helper rejects abstract ownership and grants parley only at committed contact; ordinary npc::move can evaluate guaranteed_hostile earlier. This is a source-contract mismatch; the exact native attitude writer is not directly recorded. Reproduce actual callers before the smallest correction rather than resetting attitudes.

`open_live_bandit_shakedown_surface` currently builds an avatar-backed pool and uses avatar payment; some callers require player_contact. Remote camp response therefore needs coherent recipient/payment context, not just removal of a condition.

Original evidence: [R065 extraction](../build_logs/first-smoke-065/native-r051-8820-callback-trace/first-transition-actor-operation-extraction.json), [R066 checkpoint](../build_logs/first-smoke-066/quiet-review-checkpoint.json). Owner discussion and corrections: [direction](../.de67/task-logs/early-shouted-demand-owner-20261003/owner-direction.md), [local simulation boundary](../.de67/task-logs/review-owner-235bdc799fba/owner-latest-local-boundary.md). Source anchors: `hostile_operation_player_relationship_for`, `npc::guaranteed_hostile`, `npc::move`, `npc::on_attacked`, `open_live_bandit_shakedown_surface`.

## Required native playtest arms

Josef explicitly requires (1) overmap-only waiting through time/save/reload and one resumption after actual local activation; (2) actual basecamp-NPC contact with avatar on an adjacent OMT, camp-response menu and camp-local payment/departure. Use the [native suite procedures](../.agents/skills/caol-harness/references/playtest-suite.md#bandit-waiting-and-camp-npc-contact). Adjacent OMT does not itself establish unloaded state, and direct avatar contact cannot substitute for NPC-received contact. Both arms are pending, not completed proof.

For encounter variants and the smaller cannibal projection, use the [native coverage checklist](../.agents/skills/caol-harness/references/playtest-suite.md#encounter-variability-simple-coverage-checklist). Added native cases include pre-demand attack, third-party interruption, indoor audible/unheard and empty camp. This coverage extension does not grant unrelated cannibal behavior changes or auto-battle implementation.
