# Harness qualification: current frontier

Goal: fresh Luna agents can independently understand, act, investigate, recover and report on gameplay supporting CAOL. Comprehensive DE67 package design comes afterwards. This is a transient harness capability map, not a second product ledger; `.de67/DFS.md` remains the product contract.

## Qualification boundary (from the original goal)

Deliver the reusable harness for a later DE67 playtest package. Qualify controls/observations with
representative Luna play, including ordinary failure and cleanup. Do not perform the comprehensive
CAOL feature campaign here. Starting an ordinary fire is harness qualification; certifying every
faction response to fire is later package work. CPU/RAM reading and useful comparison belong here;
full stalking-bandit diagnosis/fix is a later product task unless it blocks harness qualification.
Likewise, establish that NPC integration can be exercised and observed; detailed model comparisons
and exhaustive dialogue outcomes are later. Existing and new required primitives still need actual
behavioral checks; this boundary does not turn source inspection into usability proof. Actual E2B and E4B NPC-route
exercise belongs here; exhaustive model comparison does not.

## Current handover (2026-09-05, active qualification)

The active source is `/Volumes/CodexBulk/Schanigarten/workspaces/Cataclysm-AOL-hostile-ecology-dev`,
branch `dev`, pushed checkpoint `1c982cc432`, with the reload/terminal/setup repair described below pending.
Windows remains behind and has only portable scratch tests. DE67 campaign state is unchanged.
Live games remain sequential. The persistence trial saved and exited natively, then exposed a reload
startup defect; its replacement was explicitly quit after preserving the saved world. No game is
currently running. Saved-state comparison, actual combat damage/reload, E4B and sustained operation
remain open. Named E2B endpoint failure/recovery has current live evidence.

Recent checkpoints: `913e01acf7` makes Ollama requests nonthinking and rejects leaked reasoning;
`945df62038` exposes native pickup denials and exact item details; `1fe6f52b31` preserves nested-owner
macro interruptions; `7a456f4da6` separates failed operations from explicit quit; `f74bc6555a` adds
native diagonal/elevation actions; `8875adfd3b` adds exact visible-creature debug removal;
`fb3177fc19` repairs explicit quit, post-relaunch client generations and quoted-message retrieval;
`7665a97ed2` adds cooperative request cancellation and truthful native melee outcomes;
`7d3bc27349` binds NPC-initiated dialogue to the semantic manager; `a23d744c8a` permits
native ground wield/wear independently of pocket capacity and keeps exact UID targeting.
`30d8b9d31a` repairs native trade titles and exposes parties, values, quantities and pane switching;
`16c9290be2` makes explicit quit end the whole scenario, and `9d4c69cef8` clarifies the player skill.
Earlier source changes add NPC/player/item inspection, zones, owner dispatch fixes, CPU/RSS samples
and disk-backed exact history. Source-binding builds and narrow native tests are retained below.

The lifecycle contract supersedes the early experiment guards: a failed action, observation,
transport request or client disconnect leaves the game running. Uncertain authority revokes stale
input grants and requires a fresh observation. Only the player's explicit native quit, run.quit,
run.finish or requested bridge cleanup ends the owned game. The automatic RSS/time guard was stopped
during the camp trial; it must not be used to relaunch or end future games.

Fresh fire and NPC trials exposed reporting errors: both final witnesses missed later native
success. The exact World facts already contained the outcomes. The witnesses remain unchanged;
separate engineering judgments below correct them. `messages` now retrieves quote-bearing native
messages directly, and `controls` names the shared NPC logs. A fresh truthful player report after
these changes is still required; mechanical witness acceptance alone is insufficient.

Current combat/cancellation checks: 115 focused Mac Python tests passed, including actual adapter
wiring with native request files and parsed receipt traces. The cancellation tests cover before-input,
after-input uncertainty, a durable receipt racing cancellation, stale run/request/generation markers,
and continued observation/action. Windows passed the 36 client/movement tests plus 2 applicable
cancellation cases (2 pipe-specific cases skipped); WSL passed all 115 after a test-only Path.stat
recursion repair. A clean structured review is retained as cancel-combat-review.json. The new
native melee regression passed 48 assertions in 3 cases; movement API semantics remain unchanged.
The full Mac cockpit suite passed 254 tests. Ground equipment passed 114 assertions in one native
test case, including a ground bystander that must remain unchanged. The central lifecycle suite passed 249 Mac tests, with shared and
process-retention checks on Windows/WSL. Native pickup evidence is 265 assertions in 8 cases;
navigation 24 assertions in 2 cases; exact creature removal 51 assertions in 1 case. Mac builds
match the committed source. Native gameplay changes have Mac evidence and portable source; a
Windows/Linux game build of this frontier has not been claimed. One broader legacy adaptive-test
invocation still has four pre-existing call-signature failures, retained in its log. Relevant narrow
finalization checks passed. Structured reviews found real stale-grant and malformed-quit gaps;
both were repaired and the final reviews were clean.

### Reload, terminal and setup repair (2026-09-05)

The initial persistence session `selected-0823524f76cd479fb3c87a648c8b3b35` saved and exited
PID 93389 natively (exit 0). The configured relaunch omitted `semantic_only_startup` and
`suppress_profile_startup_input`, letting legacy startup input open Actions in PID 96450.
The bridge then attempted to overwrite its already valid finish receipt while reporting missing
reentry. The startup helper now forwards both effective flags, and the bridge preserves finish
receipts and reports the registered replacement game separately from its controller child.
The client reports failed reentry instead of waiting indefinitely for it.

Engineering attachment `recovery-persistence-20260905-2329` preserved the same game and original
artifacts. Initial missing wake/request endpoints were attached by exact native launch identity;
the native cancel reached an unsupported Action-menu owner. Luna explicitly quit that instance.
This attachment is zero-credit engineering recovery, not successful saved-state qualification.
The original saved world remains available for a native-only continuation; no fixture was reinstalled.

The death/end-screen owner now exposes actor identity, native HP-death state, preview versus actual
death, suicide and position, with a native confirmation action. Its focused native test passed
56 assertions in one case (four situations). The live delayed request/terminal route remains open.
Native Linux compilation passed for the owner and test translation unit. This is not a full Linux
runtime test. Reload forwarding passed 148 Mac classification tests and 14 targeted Windows/WSL
tests. Player/reentry validation passed 30 Mac and WSL bridge tests and 25 CLI tests. The Windows
CLI/startup selection passed 39 tests. Review found and repaired live-controller cleanup after
failed reentry; both still-alive and exited controller shapes now have subprocess coverage.

At Josef's request, derived harness fixtures remove only Giuseppe Bachman, NPC ID 4. The four
scenes inheriting him use those manifests; cannibal and ergonomics scenes have different fixtures.
Historical fixture bytes remain unchanged. Exact removal and bystander checks were performed on a
disposable copy; `giuseppe-setup-validation.json` retains the full bystander payload hashes and
unchanged source hashes. This is setup intervention and supplies no combat or faction-behavior credit.

The initial persistence cost artifact is `persistence-initial-reload-costs.json`: 744 seconds of
owned-game samples, peak RSS 989,954,048 bytes; 49 native-action samples with median 0.138 seconds
and a 15.03-second failure maximum. Reconstructed verified response views had median 7,785 and
maximum 19,104 bytes; retained responses 3,785,234 bytes and SQLite/WAL 11,210,752 bytes. These are
observed finite-run costs, not player-model token counts or a stability threshold.

## Evidence handles

All sessions below are under `.userdata/openclaw_harness/bridge-sessions/`; terminalization records point to full probe reports. Artifacts preserve exact identities; shorthand labels here are navigation only.

- **Wait baseline:** `selected-2377af5e3a174eebbd80b15585045a51`. Luna chose the alarm/wait menus and a 20-second wait, then observed game time advance. Encountered stale save/quit authority. Mechanically valid, inconclusive/repair witness. Clean finish. Peak combined resident memory ~1.40 GiB; 262 seconds.
- **Engineer exit baseline:** `selected-2dc9fc956eb342ea911c006932a08b4f`. Native save/quit, main menu quit, confirmation and process exit code 0 observed. Dispatcher discarded a real final receipt because there was no successor frame. The later fresh camp trial below closes final-receipt collection. Peak ~1.20 GiB; 182 seconds.
- **Camp startup failure:** `selected-ebb381ef49884bb08fbb3d403f210274`. No game started. A generic missing-descriptor error hid `contract_preflight_rejected`: the blanket observer trait policy demanded six debug traits absent from the prepared camp. Repaired to require declared setup facts; malformed and explicit unmet requirements still reject. Compact error now names cause and retained artifact.
- **Luna camp baseline:** `selected-84e976320a354d41ac494e089a685bf5`. Report `.userdata/harness-camp-freeplay/harness_runs/20260905_140725_869d8ba3a0b24e29a582e37493838860/probe.report.json`. Luna found friendly Katharina Leach and Robbie Knox; conversation produced “begins to follow you”; Robbie equipped a quiver and Glock in native messages. Rules screen exposed only Done. Zone-2 disable changed enabled to false/revision 1, but re-enable rejected `stale_revision`. Other requests hit wrong-surface rejection after transient dialogue returns. Witness mechanically accepted, inconclusive/repair. Cleanup `already_exited`, `safe_to_cleanup`, no owned processes; native-exit credit false. Duration 317 seconds; peak combined RSS 1,402,160 KiB (~1.34 GiB). Fixture/setup is zero-credit and these observations are not comprehensive product acceptance.

- **Fresh camp controls:** `selected-5fad555dbc004c89af6b1b03039cf47d`. Luna toggled allow_pick_up (enabled true, override_enabled true); reset changed value to false but incorrectly retained the active override. Native Default shared the same flaw. Zone-2 disable/re-enable succeeded with revisions 1 then 2. Camp actions lacked actor names and attempts encountered stale/incorrect current owners. Witness inconclusive/zero-credit; safe cleanup, no owned PIDs. Duration 361.5 seconds, peak 1,264,656 KiB (~1.21 GiB).

- **Fresh camp items:** `selected-95010883d95f43c0967fb3d8d69780c8`. Luna inspected smartphone (UPS) details, including 56 charges and native functions; toggled and restored Katharina's avoid_doors rule; disabled/re-enabled zone-2. Named camp action was accepted but returned World without opening its selector, motivating the late-dispatch repair above. Witness mechanically accepted, zero-credit; cleanup `already_exited` / `safe_to_cleanup`, no owned PIDs. Report `.userdata/harness-camp-freeplay/harness_runs/20260905_144109_5e533bf8635748ef9d6b7bd848340b9f/probe.report.json`. Duration 397 seconds, peak 1,226,400 KiB (~1.17 GiB). Reported save/quit confirmation is not yet independently confirmed as full application exit with code 0.

- **Fresh NPC/camp/performance:** `selected-a0b8752b0647429bbb78e28f90d2ed30`. Luna identified Katharina as friendly/uninjured, wielding sharpened rebar; diagnostic camp resident/activity/orders and UID 104 pizza details were readable (566 kcal, nominal shelf life 2 days). Base Missions opened for Bugchaser central. Close stalled: native intent was accepted during blocked input, but the selector failed to execute the pending action after wake, preventing its deferred successor receipt. Corrected input-loop draining now awaits retest. Two comparable one-second menu samples measured 9.87% then 8.91% of one CPU core, RSS 1,113,915,392 bytes both times; this establishes measurement usability, not idle certification or a regression. Repair witness, exact-process cleanup, no owned PIDs/native-exit credit. Report `.userdata/harness-camp-freeplay/harness_runs/20260905_153200_dfd8e93a879140f48258522f21c0a4f2/probe.report.json`. Duration 311.8 seconds, peak 1,235,632 KiB (~1.18 GiB). Telemetry Python slice checkpoint `7aca474401`, with Windows/WSL/Mac collector/client validation.

- **Fresh items/macros:** `selected-7edb3bacf34e4cdba81bbd810f3d62e6`. Base Missions opened and closed. A raw movement macro moved east, stopped with unknown_event, and Luna explicitly moved south in the same session; a second macro showed the same false stop. The producer emitted kind=world without legacy state, while the macro checked state only; corrected descriptor recognition awaits live retest. Smartphone drop succeeded. Pickup showed the dropped UID, but toggle stalled because native selected state was not published; the pending correction adds native UID/count facts. No lighter/fuel was in the shown carried inventory, and fire was not attempted. Repair witness, safe cleanup/no native exit credit. Report `.userdata/harness-camp-freeplay/harness_runs/20260905_154659_9ca4f4ff5025406a8f90405f625b86f3/probe.report.json`. Duration 311.7 seconds, peak 1,303,392 KiB (~1.24 GiB).

- **Fresh fire/item setup:** `selected-cc55c26917a641dfa5f54a8b2562f73f`. Native debug item discovery reached the electric lighter quantity prompt, but submit and cancel stalled. The legacy prompt checked its stored result only when consuming a request, although the blocked input layer had already consumed it. Corrected result draining now has native queued/preconsumed tests; fresh live retest remains. This trial did not reach pickup, ignition or full exit. Luna also guessed parent World actions while nested in Inventory; the next compact view identifies the current owner and exposes paged target choices. Repair/zero-credit witness; cleanup safe, no owned PIDs/native-exit credit. Duration 488.7 seconds, peak combined RSS 1,304,416 KiB (~1.24 GiB).

- **Archived item/fire retest:** `selected-4906f09c953147d18cf83b220586997d`. Quantity submit succeeded. Several item-wish transitions returned intermediate World frames before the next real menu/prompt; Luna then chose stale or inappropriate World actions. The native World owner had never resumed input, motivating its retirement repair. The east macro moved one tile and opened a closed door on the next action, returning partial progress 1/2 with current World control. No ignition or pickup completion was established. Fresh status and resource readings were usable. Inconclusive/repair zero-credit witness, safe cleanup, no owned PIDs; no full native exit credit. Duration 440.6 seconds, peak combined RSS 1,476,032 KiB (~1.41 GiB).

Compact response-body measurements (not complete CLI traffic or model token accounting): first camp trial 21 responses / 85,638 compact bytes / median 4,166 / max 12,295 versus 1,301,906 retained raw bytes; second 41 responses / 311,587 compact bytes / median 5,623 / max 13,920 versus 8,309,661 raw bytes. Retained evidence and repetition still need sustained-run measurement; compacting alone does not bound memory.

Historical guard receipts are retained in `build_logs/20260905-harness-ergonomics/`.
Their former time/RSS limits are superseded by Josef's explicit-quit-only requirement above.
Current resource monitoring observes without terminating. Short low-RAM trials remain short trials;
sustained stability and system memory pressure still require measurement.

## Additional user requirement: performance visibility

Josef requested CPU/RAM measurement and comparison, especially when a stalking bandit is visible.
High CPU during wait/fast-forward can be normal; distinguish it from time spent awaiting input and
from costly ordinary actions. Add bound interval CPU, resident RAM, action latency and game-time
progress with operation context, retained exact samples and compact comparison. Compare equivalent
controls with/without the visible actor; debouncing is a hypothesis, not an established diagnosis.
Do not invent universal resource thresholds or treat absent readings as zero. Existing R009
collectors provide a starting point, but macOS ps %cpu is not a true interval measurement.

## Latest behavioral evidence and corrected judgments

All session IDs below resolve under `.userdata/openclaw_harness/bridge-sessions/`.

- **Camp selection, zones, movement and native exit:** `selected-7067552a0870411a8dd2491c3220f8b2`.
  Luna independently selected the camp, created named Patrol zone 23 with exact bounds, encountered
  a disabled select refusal, disabled and deleted it. Native diagonals changed position. Actual
  level-up/down dispatch refused ceiling/no-lower-route without position change. Quicksave reported
  saved; native save-quit, main-menu quit and confirmation produced process exit 0. The 83-entry
  journal and accepted witness preserve the receipts. This closes live terminal-receipt collection;
  a new-process reload and successful stairs traversal are separate, still open evidence.
- **Ordinary fire and recoverable failure:** `selected-99fc4ad7935748efa59fb34f163ecd82`.
  Debug item creation supplied an electric lighter and paper, never a fire. Native drop and item
  activation first returned nothing-to-light. Luna changed position and activated again. Frame 53,
  response `play-7afd7a7775f048118a3d9cb10a168a3a`, records successful ignition and north-tile
  `fd_fire`/`fd_hot_air1`, with adjacent heat. The original witness incorrectly reused the earlier
  refusal; it is preserved as a reporting failure. Ordinary ignition and heat are supported;
  lighter charge consumption was not observed. Explicit run.finish cleaned up; native-menu exit
  is not claimed. Over 388.24 seconds, 52 native action intervals had median 0.13985 s and max
  0.22947 s; game RSS ranged 1,103,429,632-1,146,372,096 bytes, ending 1,128,464,384 bytes.
- **Actual NPC E2B exchange:** `selected-c330207853ae4a688ee21162ceb846ac`.
  The native utterance to Katharina reached the real runner. Frame 10, response
  `play-5b62b3ed1ac84bb798912872113a0c87`, records her spoken reply and `follow_close` output.
  Matching central prompt/runner logs show local Ollama E2B, 27 generated tokens and about 3.31 s
  generation, with thinking disabled. The original witness missed the later World message and
  searched the wrong profile logs; preserve it and this correction separately. Actor inspection
  at frame 5 preceded the reply and cannot establish a post-reply follow failure. Resulting orders
  and endpoint failure/recovery remain open. Explicit finish cleaned up; no native-exit claim.
- **Broken staged combat fixture:** `selected-a75257de12c040009fbd51097a24fcd6`.
  `bandit_live_world rejected loaded local projection claims` occurred during load. The HUD became
  visible, but startup proof stayed red and zero player requests ran. Luna explicitly cleaned up
  after preserving the failure. No combat evidence. The old draft scenario is preserved at
  `build_logs/20260905-harness-ergonomics/camp-defense-before-setup-repair.json`; the current draft
  uses the working prepared camp to qualify reusable combat/debug controls without staging a
  shakedown. The rejected fixture remains a later product/setup question.

- **Combat control diagnosis and exact removal:** `selected-2f627c8705df4d7096dc3ed684b647c2`.
  Fresh Luna spawned a zombie dog and exactly removed `process:0x16eafb418`; native frame 13 shows
  the target gone, both camp NPCs preserved, and the zero-credit intervention's HP/death record.
  A second dog's adjacent melee request `play-312b211747a449e6b0e54b6a806a9bb3` was mislabeled
  blocked. The actual successor frame 26 (`play-cf93c9f748a7499da6553349f9a6550d`) says "You miss."
  The native movement function returns false after attacking to cancel auto-travel; the harness
  had mistaken that for action refusal. The player's no-attack conclusion is superseded by this
  native record. A performed-attack outcome and visible-actor health facts are now present, with
  fresh damage/reload play still required. Explicit quit terminated only the owned game PID 73995.


- **NPC-initiated dialogue and exact NPC removal:** `selected-6467d3b45d834f0f9351a1db73045d8c`.
  Fresh Luna spawned Jae Knapp (character 18); a movement turn then entered his native Dialogue.
  Response `play-3c981790c37f49e99937400866d375b3` binds the correct speaker. Luna navigated its
  choices and confirmations, recovered one wrong-surface rejection with a fresh look, and returned
  to World (`play-b1f59045faec4a28a35494c1ca717e6a`). The prior combat session
  `selected-f97f1ee060aa4ec3a29c2322e3aca184` had timed out at this same class of spontaneous
  dialogue because it arose outside handle_action. Native manager binding is now live-qualified.
  Luna exactly removed character 18 with `play-85b23d291f4246e6aaf5cc872eb640cc`; camp actors 2/3
  remained. This intervention earns zero ordinary-combat credit. A pocket-denied ground revolver
  exposed missing Wield advertisement; its repair has native tests and awaits fresh gameplay.
- **Persistence draft setup:** two pre-descriptor attempts failed before gameplay because the new
  runtime profile lacked options and then a redundant hidden legacy option was in the override
  list. `LLM_INTENT_USE_API` defaults false and is not serialized; `LLM_INTENT_BACKEND=ollama`
  already selects the intended backend. The redundant override was removed, and the real strict
  override function now applies all requested settings. Setup failures remain zero-credit records.

- **Actual E2B failure/recovery and live player handoff:** `selected-91a059c8c23c4c7f9cfa0f5d7ec1b640`.
  The initial 21:57 HTTP 503 was startup prewarm, not a named NPC request. Later, Luna's actual
  question at sequence 101 reached Katharina; frame 95 contains her reply and `look_around`.
  Sequence 108 (`play-2a0516cf86374222a2ec87e9221ba0fe`) asked her current assignment during a
  second deliberate outage. Frame 100 (`play-3c9e3505387d47fea4849ffcab74db9b`) explicitly reports
  her HTTP 503 failure. After restoration, sequence 114 (`play-871adf375b3a44ccaa823fcdba801463`)
  asked whether she was ready to keep watch. Subsequent messages contain her new reply and
  `follow_close`. These actual named success/failure/recovery observations are player-reported and
  agree with native messages and retained proxy requests in the same game PID 85529. Orders and
  persistence require their own resulting-state checks. A fresh Luna continued from the durable
  client, reached Trade, then explicitly quit after malformed native descriptors. That quit
  terminated PID 85529, but the bridge incorrectly attempted declared reentry and failed with
  stale_response_identity. No exchange or new-process persistence was completed. Both owner-title
  and whole-scenario quit defects now have focused fixes and a fresh gameplay retest underway.
  The claimed follower approach is not supported: Katharina remained at [3368,994,0] while the
  player moved from [3372,996,0] to [3369,995,0]. Relative distance alone was misleading.
  The first cold response took 14.25 s at Ollama (10.85 s model load); follow-up look-around took
  1.41 s. An active loaded-model snapshot reports 3,553,236,088 bytes allocated by Ollama and 32%
  system memory free, with 69.69 MiB swap used. Model allocation, process RSS and system pressure
  are separate measurements; no universal resource threshold is inferred.

Trade/quit validation: `trade-native-tests-json-reader.log` passes 180 assertions with exit zero.
The initial test logged unread JSON fields; the test reader was corrected without suppressing game
errors. `trade-quit-review.json` is a clean structured review. Mac and WSL pass 32 bridge/startup
tests; Windows passes the four startup tests. The complete bridge subprocess suite uses POSIX
FIFOs and is not a native-Windows route; an attempted Windows run was stopped after `os.mkfifo`
was unavailable. Linux curses compiles the changed trade owners and test translation unit, with
the pre-existing npc.h GCC changes-meaning diagnostic demoted from error. Exact commands/logs
are retained under `build_logs/20260905-harness-ergonomics/logs/`; this is compile evidence, not
a full Linux game/test-binary run. The Mac game is rebuilt and source-bound at 9d4c69cef8.

The ended E2B/persistence session retained 166 verified default response views: median 17,587 bytes,
maximum 17,956 bytes, total 2,206,973 bytes. Retained response files total 16,748,599 bytes and
SQLite/WAL 46,367,088 bytes. Over 1,400.73 seconds, game RSS peaked at 1,123,090,432 bytes;
136 action intervals had median 0.17989 s and maximum 15.06608 s (the failed trade transition).
Later compressed/low-RSS phases are not comparable idle baselines. These reconstructed response
views are not complete CLI traffic or model token counts; player-model token costs are unavailable.
Artifact: `persistence-ended-costs.json`. The owned outage proxy was terminated after the trial;
Ollama remained running and had no model loaded at cleanup (`endpoint-proxy-cleanup.json`).

## Capability matrix

| Capability / entrypoint | Current behavioral evidence | Remaining qualification or later product question |
|---|---|---|
| Scenario discovery / registry-query | Fresh Luna selected camp, refined a mismatch and launched bound authority; broken combat fixture gave a concrete load cause | Changed combat setup and persistence route are being exercised |
| Player/world perception / look, inspect, messages | Named actors, player position/health/needs/equipment, neighbours and exact NPC/item diagnostics used in play; retained fire/reply found by current messages CLI | Fresh player must reconcile later outcomes in its own witness |
| Navigation / World, movement macros | Cardinal/diagonal position changes; closed door partial progress and recovery; actual ceiling/no-lower-route refusals | Successful elevation/overmap crossing if needed by selected ecology route |
| Wait / native owners, game.* macros | Native short wait advances game time; movement macro interruption preserves state | Sustained wait, explicit pending interruption/resume and meaningful progress costs remain |
| Inventory / inventory and item owners | Exact details, native debug quantity, wield/drop/pickup; pocket denial; actual lighter activation | Representative weapon/reload and completed transfer/trade remain |
| Fire / ordinary item activation | Failed attempt then successful native ignition and adjacent heat in fire session frame 53 | Maintenance/consumption observations only as later product route requires |
| Speech/dialogue / world.chat | Ordinary conversation, arbitrary utterance, delayed actual model reply, nested cancel paths; fresh named E2B success/failure/recovery correctly reported | Actual E4B route remains; detailed dialogue behavior is later product work |
| NPC orders/rules / dialogue, rules owner | Follow command and avoid_doors rule toggle/reset; exact actor state accessible | Post-order follow/stay movement and persistence remain |
| NPC inspection / actor identity | Actor-bound health, orders, equipment, exact carried-item details used by Luna with provenance | Cross-process stable identity comparison remains |
| Combat / movement, fire, targeting | Native miss/performed-attack receipt and exact visible target HP are observable; staged fixture failed before control | Actual melee/ranged damage, reload, fleeing and death/terminal recovery remain |
| Trade / native dialogue/trade | Live title defect repaired; actual native trade test confirms party identities, selected quantity, two-way transfer and cancellation (180 assertions) | Fresh Luna completed transfer and cancellation remain; full shakedown branches are later product tests |
| Camp / world.basecamp_missions | Named assigned-camp identity and actual selector open/close used by Luna | Establishment, mission execution and routing belong to package-selected product claims |
| Zones / world.zone_manager | Toggle with revisions; fresh named Patrol create/bounds/disable/delete and disabled-select refusal | Persist one identifiable zone; Locker/Patrol/Food/Storage NPC use is later product work |
| Ecology / observations and exact logs | Source and fixtures available; exact log query machinery works | Fresh usable identity/state retrieval over time/distance remains; full faction lifecycle is later |
| Debug setup / debug and world.debug_kill_creature | Native item setup and exact zombiedog removal; fresh second Luna removed spawned NPC character 18 while camp actors 2/3 remained | Setup earns zero ordinary-combat credit |
| Save/reload / native save/menu/process | Camp Luna quicksave and full native exit 0 with retained final receipts | State comparison in new process and continued client use remain |
| NPC local LLM / actual runner route | Actual E2B named success/HTTP503/recovery in one game; spoken reply, generation/load metrics and model memory; think:false | Actual E4B route remains; detailed comparisons belong to later package |
| Evidence / journal, inspect, finish | Exact typed journals accepted; archive/messages recover real contradictions; explicit cleanup works | Fresh accurate witness after interface clarification remains |
| Performance / performance | Bound CPU/RSS comparisons used; fire action latencies and 6-minute RSS interval retained | Sustained comparable idle/action/wait costs and pressure remain; visible-bandit optimization is later |
| Lifecycle / bridge, quit, cancel | Live ordinary fire failure recovered; explicit native/finish cleanup; actual subprocess tests cover malformed observations/receipts, disconnect retention and explicit quit | Live pending interruption and declared post-relaunch continuation remain |

Implementation status, Luna usability and product verdict are separate. Unresolved required
primitive gaps remain open. Row examples are discovery leads, not an exhaustive product campaign.

## Product questions retained for later package design

Living NPC intent/context and follow/camp routing; Locker/Patrol/Food/Storage behavior; bandit natural stimulus/scouting/demand/payment/refusal/return; cannibal shared discovery/day hold/night departure/approach through dawn; light/smoke/sound positive and negative controls; world boundaries and persistence; flesh raptors and integrated performance. Use the DFS and actual CAOL/upstream delta to reconcile coverage. Stalkers and zombie riders remain excluded pending the owner's design decision.
## Retention discovery

A synthetic capacity experiment using the retained real 70,674-byte World descriptor measured
902,415 / 4,578,383 / 9,173,441 bytes of retained Python allocations after 10 / 50 / 100 fresh
observations. Both `_observations` and `_transcript` grew by one record per frame. This is structural
retention evidence, not a gameplay memory benchmark. Exact artifact:
`build_logs/20260905-harness-ergonomics/retained-frame-memory-experiment.json`.
Disk-backed history checkpoint `80b720cce2` retains about 0.30 MB across the same 10/50/100-frame
experiment, including seal/finish/export, with exact citations and legacy JSON parity. A synthetic
14.43 MB full-report registry ingestion measured 2.53 MB peak Python allocations and 0.20 MB retained;
its reference/composition consumer changes remain under review. These are synthetic capacity checks,
not long-game RSS measurements. No history deletion or universal action cap is implied.
