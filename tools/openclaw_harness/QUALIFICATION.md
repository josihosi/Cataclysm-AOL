# Harness qualification: reusable capabilities

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

## Current qualification status (2026-09-06)

Canonical source: `/Volumes/CodexBulk/Schanigarten/workspaces/Cataclysm-AOL-hostile-ecology-dev`,
branch `dev`, pushed code checkpoint `54d6c00dfe`. Windows remains behind. Fresh Luna players have
used the native CLI for camp interaction, fire, trade and persistence, actual combat, local E2B/E4B
NPC replies, interruption recovery, ecology-log investigation and explicit cleanup. The repaired
post-death route now reaches a truthful unsupported MESSAGE_LOG owner after the native choices.
Fresh ordinary wearing and subsequent takeoff now have native action, message and item-location evidence.
The player corrected a reversed chronology in its sealed witness; the actual receipts remain authoritative.

Implementation tests, fresh-player usability and CAOL product verdicts remain distinct. The table
below is the current capability summary; dated sections preserve earlier failures and their later
repairs. Full faction lifecycles, prolonged guard/equipment behavior, a matched visible-bandit
performance comparison and full Windows/Linux game runtime remain later product/platform checks.
No comprehensive DE67 campaign or clock changes were made.

## Capability matrix

| Capability / entrypoint | Behavioral evidence | Limits / next product check |
|---|---|---|
| Scenario discovery / registry-query | Fresh Luna selected camp, refined a mismatch and launched bound authority; broken combat fixture gave a concrete load cause | Fresh corrected setup, native-only outer finish and saved-world continuation succeed; full product scenario selection remains package work |
| Player/world perception / look, inspect, messages | Named actors, player position/health/needs/equipment, neighbours and exact NPC/item diagnostics used in play; retained fire/reply found by current messages CLI | Fresh combat and NPC players retrieved later outcomes; preserve the earlier witness corrections |
| Navigation / World, movement macros | Cardinal/diagonal position changes; closed door partial progress and recovery; actual ceiling/no-lower-route refusals | Successful elevation/overmap crossing if needed by selected ecology route |
| Wait / native owners, game.* macros | Three native 5-minute waits advance 15 game minutes; movement interruption preserves state; pending pause cancelled after 283 actions and play continued | Sustained-run measurements are finite; no universal stability or resource threshold |
| Inventory / inventory and item owners | Exact details, native debug quantity, wield/drop/pickup, ordinary shirt wear/takeoff, pocket denial, lighter activation, firearm load/unload and restored bench Glock inspection | Wearing is confirmed by the put-on message and later worn location; NPC equipment assignment is later product work |
| Fire / ordinary item activation | Failed attempt then successful native ignition and adjacent heat in fire session frame 53 | Maintenance/consumption observations only as later product route requires |
| Speech/dialogue / world.chat | Ordinary conversation, arbitrary utterance, delayed actual model reply, nested cancel paths; fresh named E2B success/failure/recovery correctly reported | Actual E4B companion replies and runner metrics are retained in the evidence history; detailed dialogue behavior is later product work |
| NPC orders/rules / dialogue, rules owner | Follow command and avoid_doors rule toggle/reset; exact actor state accessible | Saved follow_close override survives reload; actual following travel is observed. Guard order/state and one pause at its post are observed; longer guard behavior is later product work |
| NPC inspection / actor identity | Actor-bound health, orders, equipment, exact carried-item details used by Luna with provenance | Katharina character:2 and Robbie character:3 remain identifiable after native save and later continuation |
| Combat / movement, fire, targeting | Fresh native melee 36→24 HP, revolver kill/ammo consumption, reload/unload, movement away from a live hostile and actual avatar death | Actual debug death reaches available follower/watch prompts, then truthful unsupported MESSAGE_LOG after NO/NO; affirmative branches and broader combat are later tests |
| Trade / native dialogue/trade | Native test confirms parties/counts/transfer/cancel (180 assertions); fresh Luna canceled one offer and completed a later exchange | Native restored pickup/details locate the Glock on its bench; full shakedown branches are later product tests |
| Camp / world.basecamp_missions | Named assigned-camp identity and actual selector open/close used by Luna | Establishment, mission execution and routing belong to package-selected product claims |
| Zones / world.zone_manager | Toggle with revisions; fresh named Patrol create/bounds/disable/delete; reloaded Food zone-2 remains disabled at revision 1 | Locker/Patrol/Food/Storage NPC use is later product work |
| Ecology / observations and exact logs | Fresh Luna retrieves exact cannibal candidate and separate bandit no-signal records over 15 game minutes and movement, with run/site/actor provenance | No natural cannibal stimulus-to-raid or routing-defect verdict; full faction lifecycle is later |
| Debug setup / debug and world.debug_kill_creature | Native item setup and exact zombiedog removal; fresh second Luna removed spawned NPC character 18 while camp actors 2/3 remained | Setup earns zero ordinary-combat credit |
| Save/reload / native save/menu/process | Native save/exit 0; later processes retain exact NPC identity, changed follow rule and disabled Food zone | Native restored bench item, NPC and zone observations support persistence; continuation finishes without a spurious extra reentry |
| NPC local LLM / actual runner route | Actual E2B named success/HTTP503/recovery in one game; spoken reply, generation/load metrics and model memory; think:false | Google E4B QAT companion route also exercised; detailed comparisons belong to later package |
| Evidence / journal, inspect, finish | Exact typed journals accepted; archive/messages recover real contradictions; explicit cleanup works | Fresh combat witness and same-request recovery of a temporary archive-read error succeeded; mechanical validity still needs causal judgment |
| Performance / performance | Bound interval CPU/RSS, action latency and comparison controls used; finite pause/duration-wait, independent controller and host samples retained; matched parser memory comparison | No matched action-only or with/without-visible-bandit CPU benchmark; player-model tokens unavailable |
| Lifecycle / bridge, quit, cancel | Live ordinary fire failure recovered; live pending cancellation and continued use; explicit native/finish cleanup and subprocess retention checks | Post-relaunch cleanup and truthful post-death ownership/explicit finish succeed; full post-death viewer controls remain unsupported |

## Retained evidence and repair history

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
messages directly, and `controls` discovers bound native/transition and shared NPC logs. The later E2B outage/recovery and combat witnesses agree with the resulting native messages; mechanical witness acceptance alone remains insufficient.

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

## Reload continuation and native-only dependency repair (2026-09-06)

The native save/exit from `selected-0823524f76cd479fb3c87a648c8b3b35` remains valid. Its first
replacement exposed omitted native-only launch flags and then an unowned legacy Actions menu.
After explicit quit, a continuation loaded the same saved profile without a fixture reinstall but
lost its controller to a missing Peekaboo executable. The game stayed alive. Engineering attachment
`recovery-saved-world-20260905-2352` restored the existing native transport in PID 2336; attachment
gets zero play credit. Luna then made 31 requests and explicitly quit. Controller child exit 0,
`safe_to_cleanup`, and the dead game PID agree.

The completed native trade and its cancellation are separately established. Original sequence 24
selected NPC-side Glock UID 3994; sequence 25 canceled; sequence 29 reopened the same NPC-side offer
with no selection. The later accepted exchange removed that offer from the NPC pane. Sequence 32
opened “Stop wielding six-shooter?” and Luna canceled that placement prompt. A player-side trade
pane later listed Glock UID 4409, but that pane also includes nearby items. This did **not** establish
carried possession. The saved player has no Glock; the unchanged saved map has `civilian_glock19`
at [3365,990,0], the saved player's tile. `persistence-trade-causal-review.json`,
`persistence-saved-payload-audit.json` and `persistence-nearby-saved-items-audit.json` preserve the
exact records. Fresh loaded pickup/location inspection is still pending. Current combat-profile
weapon observations belong to a different trial and must not be used to fill this persistence claim.

The reloaded native NPC inspector confirms Katharina (`character:2`) with `follow_close=false`,
base false and override enabled, matching the deliberately saved rule change. During subsequent
ordinary southeast movement, her absolute position changed from [3364,990,0] to [3368,994,0]
while the player reached [3374,999,0]. This is actual NPC movement, unlike the earlier misleading
relative-distance claim. The next southeast movement was blocked. The saved `zone-2` is still
Basecamp: Food, disabled with semantic revision 1 and bounds [3361,999,0]–[3364,999,0]. World lists
omit disabled zones; its absence there was not a deletion. Fresh manager inspection remains.

Fresh scene variants omit Giuseppe (`character:4`). After PID 2336 exited, Josef's requested exact
removal was also applied to this retained saved world. Only `overmaps/o.0.0.zzip` changed; the full
payloads of NPCs 2, 3 and 5–17, player save, map items and zones stayed unchanged. A backup and
complete before/after hashes are in `persistence-giuseppe-removal.json`. This is setup, with no
combat or NPC-lifecycle credit.

Checkpoints `2cdd48f9c2` and `7f49f6dcd8` preserve immutable finish receipts, report retained replacement
processes truthfully, forward native-only reentry settings and add the native terminal owner.
`0c14ad2187` skips startup screenshots/OCR on the native-only route and uses validated native World
readiness while retaining log/process errors. A live combat launch then exposed three outer probe
captures still calling Peekaboo; `07743a9ce8` forwards the same setting there, including final capture,
and respects explicit screen-observability requirements. The skill now distinguishes trade offers
from carried items and visible zones from manager state. Both scoped reviews are clean; the latest
Mac checks pass 2 capture/error-contract tests plus all 148 proof tests; Windows and WSL each pass
16 applicable capture/startup tests. The repaired full outer route still awaits a clean live run.

The recovered persistence interval spans 446.01 seconds with 50 owned-game samples, peak RSS
888,979,456 bytes, and 22 native action intervals (median 0.1596 s, maximum 0.1866 s). Its 31 verified
response views have median 15,452 bytes and maximum 18,959 bytes, totaling 421,279 bytes. Retained
response files use 3,080,069 bytes; SQLite/WAL uses 10,674,160 bytes. These are reconstructed views,
not complete CLI traffic or model token costs, and do not establish indefinite stability.
Artifact: `persistence-recovery-costs.json`. The owned Linux test container was stopped and removed;
its host build/evidence artifacts and all other containers were preserved
(`linux-test-container-cleanup.json`).

## Native menu wake repair and E4B evidence (2026-09-06)

Combat player 14 reached the native skill editor after four recorded melee misses against the
same zombie dog (36/36 HP). Its `allow_anykey` menu overwrote the result of a semantic selection
or cancellation with `UILIST_UNBOUND`. The accepted receipt therefore did not complete the
interaction. The player explicitly quit PID 5020; no melee-damage credit is assigned.

Commit `1225e22b64` preserves semantic callback results at the native input boundary. A rejected
queued request is distinguished from a stale transport notification, so rejection cannot become
physical ANY_INPUT either. The focused real input-boundary regression passed choose/cancel/reject
(15 assertions). The full native semantic suite passed 1,379 assertions in 60 cases after correcting
one stale inventory test that expected a rock to be wearable. Linux compilation passed for
`uilist.cpp`, `input_context.cpp` and the new test translation unit in the curses configuration.
This is compile evidence, not a Linux game run. Astyle 3.1 and the scoped structured review passed.
The source-bound Mac executable is SHA-256
`2456b8cf5e1727d1bfe4c148cbb9fd61d3d53394c7c6eccd0d74e087d3a191a7`.
The exact temporary Linux container was removed; its host logs remain.

Retained evidence under `build_logs/20260905-harness-ergonomics/`:
`uilist-wake-native-tests-final.log`, `uilist-wake-native-semantic-suite-final.log`,
`uilist-wake-linux-compile.log`, `uilist-wake-linux-test-compile.log`,
`uilist-wake-review.json`, `uilist-wake-source-bound-build.log` and `wake-linux-container.json`.
Fresh live menu/combat/terminal qualification is in progress; tests alone do not close it.

`e4b-live-runner-evidence.json` retains exact runner-log byte slices/hashes, named native response
matches and metrics for Google `gemma-4-E4B-it-qat-q4_0-gguf:latest` in
`recovery-combat-20260906-0008`. Katharina (`req_0`) and Robbie (`req_2`) responded to
“We need a clear plan for this camp.” with spoken replies and `follow_close|look_around` intents.
Runner generation took 18.932 and 6.336 seconds; the first included 12.657 seconds of model load.
Their item-selection subrequests took 2.142 and 2.082 seconds. All four actual responses ended
normally. The separate startup prewarm hit its eight-token limit and is not conversation evidence.
These establish actual route operation and observable intent handling, not model quality or
correctness of the chosen item goals. The fresh player's corrected honest witness is
`.userdata/harness-camp-defense/harness_runs/20260906_000343_d8e5a2f162734817a80a1fa7d2845a63/e4b-combat-trial.honest-witness.json`
(SHA-256 `344c7eb9dfac67734f8c0550823ee3f4c19dee5bfc096ddf256d66c38e250ec2`).
Speech request `play-920593fb4daa4d478f4ea3e08e868ef6` is correlated to reply observation
`play-19f70b00ffa64a7383ee2b78b80b9bfb` (native frame 25) and the named runner records. The four installed E2B/E4B variants have not been compared
in a matched benchmark.

A current E4B-loaded snapshot during the fresh combat scene reports 6,416,498,688 bytes
of model-server RSS, 5,374,780,373 bytes of Ollama model allocation and 37% host memory free
(`e4b-loaded-resource-snapshot.json`). This is one observed footprint, not a peak or acceptance
threshold. The ended combat-14 game had 93 bound samples over 663 seconds, native-action median
0.154 seconds, and 51 verified compact response views with median 7,653 bytes / maximum 19,387
bytes. Retained responses occupied 2,291,595 bytes; evidence SQLite/WAL occupied 9,813,832 bytes
(`combat14-ended-costs.json`). Reconstructed views are not actual full CLI traffic or model tokens.

## Fresh combat and death aftermath (2026-09-06)

Fresh Luna player 16 independently operated
`selected-bb6f0b3f2bc748389d8d99c47adf821d` (native run
`8c73e6d7f887ea14afd20189ca5e5b1eb6f19b6d3db8005de66ba3bdb5965321`, PID 10387).
The setup enemy `process:0x1634e1818` took an ordinary 12-damage melee hit (36 to 24 HP).
After recovering from an unsuitable ammunition choice and activity interruptions, Luna loaded
six .38 JHP rounds into the six-shooter, shot for 31 damage, and killed that dog; five rounds
remained. It then unloaded item UID 2641 to 0/6 and recovered the ammunition. These are native
combat/item consequences, while the spawned scene receives zero natural-discovery credit.
`combat16-shot-evidence.json` retains response hashes and exact native messages from frame 53
(shot request `play-a76ce22a91d045c1a9e124346a63b020`) and frame 56
(unload request `play-f1e8b87b4c9f49a1a379067a7b82f53f`).

Luna also moved west with a second live hostile dog (`process:0x1634e2818`) still present
(player x3370, dog x3372). This qualifies movement away from an active threat, not guaranteed escape.
The Katharina/“Bugchaser central” mission selector is ordinary camp interaction; opening it does
not prove an E4B exchange or a completed camp mission. Actual E4B replies are evidenced separately.

The player later reached terminal frame 106 with Zoraida Vick / character:1, `actual_death=true`,
`avatar_dead=true`, `preview=false`, `suicide=false`. Its delayed `terminal.confirm`
(`play-68be83ed18f848fb99c6c67b3e930293`) was ACCEPTED by the exact native owner. The next
prompt was missing because the fallback semantic manager ended with the end-screen function,
before the death-camera question in `game::is_game_over`. The old adapter discarded that accepted
receipt and reported `native_surface_receipt_timeout`. Do not read that error as native rejection.

Commit `9d72fa40b9` holds the fallback manager through the death aftermath and preserves accepted
receipts when a successor fails to appear, returning `native_surface_successor_timeout` while
revoking stale authority and retaining the game. The existing full native semantic suite again
passed 1,379 assertions/60 cases; the changed `game.cpp` compiled on Linux. Mac and WSL passed
84 relevant Python tests; the two new regressions passed on Windows. A broader Windows invocation
still exposed five old POSIX/LF-fixture assumptions and one POSIX-FD test error; those are outside
this change, not Windows runtime proof. See `death-aftermath-portable-tests.json` and associated
build/test logs. Scoped review is clean (`death-aftermath-review.json`); live next-prompt validation
remains assigned to the final player. The skill now defines native semantic CLI operation over SSH
explicitly after a context handoff briefly confused it with desktop control.

The final finish request `play-3dc776ec9a8b40f28a8f682d451672e5` is accepted and mechanically
validated; witness SHA-256 `0bb06db126510d49e6d9d1fd4888f15717ac99c7e7c59e73bf536a1685361c10`.
A temporary `archive_database_unreadable` on its first collection preserved the pending request.
Luna later collected the SAME finish successfully without replay. The bridge is `safe_to_cleanup`,
child exit 0 and PID 10387 absent. This is explicit finish cleanup, not native application-exit credit.
The exact submitted witness content is copied in `combat16-submitted-witness.json`; the canonical
response and archive remain in that session. The outer probe completed with `ok=true` without
Peekaboo; its formal feature verdict remains yellow, consistent with disposable freeplay rather
than a frozen product proof.

`combat16-ended-costs.json`: 190 bound game samples over 859.9 seconds, peak sampled RSS
671,760,384 bytes; 80 native-action timings, median 0.129 seconds and maximum 15.005 seconds
(the missing-successor timeout). All 128 archived responses now verify. Their reconstructed compact
views have median 6,401 and maximum 19,384 bytes; response storage is 4,089,189 bytes and
SQLite/WAL 12,083,200 bytes. These are finite session observations, not indefinite-stability claims.

## Earlier continuation attempts (2026-09-06; repaired below)

Session `selected-56f5ef660fb94c76b5cb1539a9d769fb` loaded the retained persistence world in
PID 17120. Native Zone Manager frame 10 (`play-01e0e9ae4f0448ef90913765069fc6cd`) confirms
Basecamp: Food / `zone-2`, enabled false, revision 1. `persistence15-zone-evidence.json` retains the
verified response hash and facts. The player inspected adjacent ground, but the underfoot choice
was misleadingly labelled “Pause”; actual traded-Glock location and stay-order behavior remain open.
Explicit finish ended the game; the bridge then incorrectly expected another reentry, under repair.

The fresh cannibal scene retained PID 17997 after its first step tried an optional screenshot with
Peekaboo deliberately unavailable. Engineering attachment `recovery-cannibal-20260906-0126`
reused the exact run/process and passed remaining saved-item/map audits plus native bootstrap.
No fixture was reinstalled and attachment receives zero gameplay credit. Luna cancelled the pending
ten-minute pause recipe after 283 native actions (`play-3163da356f474a28b2870ae034c8d1f8`), then
observed World frame 326 at game minute 8225 and continued with a native NPC inspection. The
cancellation reason “scheduler_stall” is the player's label, not evidence that game time had stalled.
The returned `not_dispatched` applies to the action at the cancellation boundary, not the 283
prior native actions. These observations qualify actual pending cancellation and continued use.

Debug head damage then caused actual death at frame 333 (zero natural-death credit).
`terminal.confirm` reached the follower question at frame 334; NO reached the watch question at
335. The final NO advanced into `turn_handler::cleanup_at_end -> game::death_screen ->
Messages::display_messages`, established by `cannibal15-terminal-stack.txt`. Its semantic manager
had ended, so the descriptor and deferred receipt were stale. Commit `7ff630b86d` now preserves
manager lifetime through cleanup; the unsupported message viewer must be reported as the current
owner instead of the old question. Full post-death viewers are not newly implemented.

Luna sealed witness `3a97a3eb1be9c8fe763e23e7064e73bef173ceba2aea47979f37aa11b32cbac7` and
explicitly finished with `play-9c42d1f2badb474f8d9df3803f7e685a`. The game and controller exited;
bridge state is `safe_to_cleanup`. This is explicit finish cleanup, not native application exit.
`cannibal15-cancellation-terminal-evidence.json` retains the verified responses and corrected
judgment. Meaningful ecology identity/transition retrieval remains for a fresh player; inspecting
a camp NPC alone does not satisfy it.

`cannibal15-ended-costs.json` contains 681 game samples across 682.8 seconds, peak sampled game
RSS 584,957,952 bytes and 337 native-action intervals (median 0.307 s; timeout maximum 15.048 s).
The 23 verified response views have median 6,085 and maximum 16,278 bytes. Retained response
files occupy 653,790 bytes and SQLite/WAL 218,434,904 bytes. During the longer pause sequence,
the controller reached about 491 MiB RSS while its raw native trace reached about 42 MiB.
Source inspection found whole-history byte loading and parsed-event accumulation before selecting
the existing recent-event window; the reader repair is measured below. These are finite costs,
not a stability threshold or player-token accounting.

## Final reader and log-discovery repair (2026-09-06)

`refresh_semantic_step_trace` now parses one line at a time up to the sampled byte boundary and
retains the existing bounded recent-event suffix while parsing. It keeps complete disk history.
On the actual 43,785,076-byte cannibal trace, `refresh_compare.py` measured Python peak allocations
of 116.164 MiB before and 4.195 MiB after, with identical 253,094-byte output (SHA-256
`5f1cbfdd2ef4977c556ca0f8f78f06916214605623cd1bf9da10445a54b459e8`). Durations were
0.534 and 0.538 seconds. This bounds parsing memory; scanning time still grows with history.
`refresh_compare.result.json` retains the comparison, not a long-game stability claim.

The launch producer publishes exact native-event, native-snapshot, transition and profile-debug
paths. Snapshot paths follow the operational process directory even when save/relaunch aggregates
other artifacts in the initial directory. Read-only `controls` exposes their availability, scope,
run/binding and executable log-query arguments; shared NPC logs omit the run filter and require
explicit correlation. Missing or mismatched current bridge identity grants no query under a false
binding. Missing metadata/files mean unavailable evidence, not absent ecology.

The current client suite passes 30 Mac tests. Eleven selected reader, capture, continuation,
producer and controls cases pass on both Windows and WSL; the two producer cases, including the
new distinct-directory relaunch regression, pass again on both after the last path correction.
The skill validator passes. `final-player-portable-validation.json` records the exact selectors
and overlapping groups. Structured review via `autoreview --mode local` is clean in
`final-player-log-review-relaunch.json`; accepted identity and relaunch-path findings were repaired.
Native source changes compile on Mac and as narrow Linux translation units; live ownership
checks remain separate below. Source checkpoint `54d6c00dfe` is pushed; its source-bound Mac build
is SHA-256 `9aab1a6f7bb0e6b981decf4cb6e489dda431de688f24d04ab13957f6e1521a4e`.
The General build matrix for that checkpoint is pending at the recorded snapshot:
https://github.com/josihosi/Cataclysm-AOL/actions/runs/34000763245 . A prior native checkpoint's
Clang-tidy-plugin workflow failed because installed LLVM 23 lit rejects `ShTest(True)`; plugin
compilation itself passed. This is retained workflow/tooling failure, not a green hosted run
(`hosted-ci-9d72-snapshot.json`, https://github.com/josihosi/Cataclysm-AOL/actions/runs/33998069154).

## Fresh saved-world continuation (2026-09-06)

Fresh Luna session `selected-7df1738e39cb498e8b5411603151abd3` loaded the retained persistence
world in new PID 30112, using the declared post-relaunch continuation without reinstalling the
fixture. Native frame 6 advertises “Here (current tile)”; choosing it opens pickup frame 7 with
Fighters' Glock 19 UID 4409. Its details at frame 8 say `Location: bench`. The avatar is at
absolute map square `[3365,990,0]`, agreeing with the prior saved `civilian_glock19` placement.
The gun is on the bench; this does not claim the avatar carries it. Native pocket and wear
restrictions remain explicit while ground wield is available.

Katharina `character:2` initially reports Following/NULL. Luna independently chooses the ordinary
Guard here order. Inspection then reports Ignoring/GUARD_ALLY, guard_post `[3364,990,0]`; World
shows her at that post through one ordinary pause while the avatar stays at `[3365,990,0]`.
This qualifies the native order/state result and observed pause. Earlier actual follower travel
is separate evidence; this trial does not establish longer guard behavior or separation travel.
`persistence18-item-guard-evidence.json` retains seven verified response hashes and exact facts.

The player discovers and successfully queries the live native and transition logs through
`controls`; each has 15 matching receipt records. `persistence18-log-discovery-evidence.json`
preserves retained record handles with a post-finish reconstruction. After cleanup the live
session descriptor is released, so current-owner controls report unavailable; the exact retained
paths and response handles still support read-only retrieval.

The witness remains inconclusive for its broad charter, with accept disposition and mechanical
validation (SHA-256 `4526ab19aa5df07303accbd12385d2004821c45bb6b6ce4e68061d2284d659e7`).
These separate engineering judgments compare its facts with earlier saved-state evidence; they do
not rewrite the witness or call the whole charter proved. Explicit finish
`play-87dd43a476cb45c995d184b4f70d8810` reached `safe_to_cleanup`, controller exit 0, with game
30112 and controllers 30083/30084 all gone. No extra relaunch was expected. This is explicit
finish cleanup, not native application-exit credit (`persistence18-finish-evidence.json`).

`persistence18-ended-costs.json` contains 33 owned-game samples over 149.9 seconds of observed
play, peak RSS 1,126,645,760 bytes; 15 native-action intervals have median 0.132 and maximum
0.144 seconds. The 21 verified response views have median 7,763 and maximum 17,864 bytes;
retained responses occupy 1,279,744 bytes and SQLite/WAL 3,866,624 bytes. Late independent
process samples also include witness-writing time and show controller RSS separately; they are
not included in the shorter gameplay sample span.

## Fresh ecology, native waits and death cleanup (2026-09-06)

Fresh Luna session `selected-6244199ee8ad4f8fa6a0aec14fe153d5` launched PID 31677 with Peekaboo
unavailable, through the corrected native-only startup and optional capture path. Three native
`wait.5m` actions crossed game minutes 8219 to 8234; one north move changed the avatar from
absolute map square `[3372,996,1]` to `[3372,995,1]`. This exercises useful duration waits without
repetitive pause requests. The earlier 283-action pending cancellation remains separate evidence.

The player used controls-discovered logs to retrieve the exact zero-credit cannibal candidate
`overmap_special:cannibal_camp@140,38,0`: at minute 8220, abstract_bootstrap registered
candidate_ready with abstract_roster 5, concrete_actors 0, target_knowledge 0, dispatch 0 and
contact 0. Distinct bandit-camp reads at minutes 8225 and 8230 report no_signal_source/no_new_lead
for `overmap_special:bandit_camp@140,51,0`, observer 4. These records can coexist. Their different
identities do not establish a routing defect; an identity-continuous cannibal stimulus/progression
link was not proved. The player's closeout correction preserves that narrow interpretation.
Shared logs lacking run correlation supply unavailable evidence, not ecological absence.

Explicit debug torso damage caused actual death at frame 16 (zero natural-death credit).
`terminal.confirm` reached the follower question; the player deliberately declined follower
takeover and the subsequent watch choice. The next frame 19 truthfully reports unsupported
`MESSAGE_LOG`, reason `unclassified_native_input_owner`, with no actions. The prior stale question
and missing successor receipt are resolved. The declined choices were available; full message-viewer
controls and affirmative takeover/watch branches were not exercised.

After correcting three rejected witness submissions, the player explicitly finished using
`play-6beb9a79604f436cbc2c81748cf70342`. The witness is mechanically valid and inconclusive,
with zero-credit/accept disposition. Its canonical witness hash is
`4df3fcb661535e61c0858def1265b26f5a49d7c20de14e5629def6298df7769f`; its stored file bytes have a
separate hash. The probe report hash is `aff42cb2a345dda8a013ff1570f168b961c6cc989adbf405ab4289ce58fd0286`.
Final bridge state is safe_to_cleanup, child exit 0; game 31677 and controllers 31629/31630 are all
gone. Cleanup has no native application-exit credit. `ecology19-final-evidence.json` retains exact
native response hashes, transition byte offsets/lengths/hashes, terminal receipts and corrections.

`ecology19-ended-costs.json` has 42 owned-game samples over 218.3 seconds of observed play:
peak RSS 1,152,647,168 bytes; 19 action intervals, median 0.130 and maximum 0.248 seconds. The
27 verified response views have median 6,565 and maximum 17,384 bytes; retained responses occupy
936,489 bytes and SQLite/WAL 2,879,488 bytes. Two player interval CPU samples measured 13.81%
while awaiting World input and 11.83% at the unsupported post-death owner (one core basis).
They are different contexts, not a matched action-only or visible-bandit benchmark.

`ecology19-process-costs.json` and `ecology19-process-samples.jsonl` independently cover about
382 seconds including witness writing. Controller RSS peaked at 49,119,232 bytes, bridge RSS at
20,299,776 bytes and game RSS at 1,152,647,168 bytes. These native duration waits produced fewer
frames than the earlier pause-loop run; their RSS difference is not a matched performance result.
The separate retained-trace parser comparison supplies that controlled memory comparison.


## Final ordinary-wearing check (2026-09-06)

Fresh Luna session `selected-a1e888a3e9d74993b9081bd896dcb046` used the disposable native
ergonomics scene on source checkpoint `54d6c00dfe`. An initial wear attempt truthfully returned
“You are already wearing that.” Luna then took off the long-sleeved shirt through its item menu
(`play-200a0662ee8d418f8afb1be540b476ba`, frame 7), rediscovered the current item UID and put it
on (`play-0beeba13d28a43d3915757500265b156`, frame 10). The new native message says “You put on
your ... long-sleeved shirt” at 05:59:56. Item details at frame 14 / J0034 report `Location: worn`,
UID 23667. A later takeoff at frame 17 leads to `Location: jeans` at frame 19 / J0045, UID 23797.
Native item moves changed UIDs; this is observed equipment state, not same-UID persistence.

The player recovered from unadvertised parent/cancel actions by observing and using the current
owner. Its sealed witness reverses the chronology of J0034 and J0045. Mechanical validation
accepted the references but does not establish that causal order. The player acknowledged the
correction in an evidence-only closeout; the original sealed witness remains unchanged. The valid
wearing claim uses the earlier actual takeoff/put-on messages and subsequent worn-location fact.
`wear20-final-evidence.json` preserves eight hash-verified responses and the correction.

Explicit finish `play-14a03cc375d940ceb4279fa666dcfbd6` reached `safe_to_cleanup`; game 34000 and
controllers 33960/33961 are all absent. This is explicit finish cleanup, with no native-exit or
product-feature credit. The canonical witness hash is
`e40ea96158243aba19bbd6b18307ef1442b87402eccb9bda0a4787c88e73f08a`.

`wear20-ended-costs.json` retains 42 game samples over 173.3 seconds, peak RSS 1,164,460,032 bytes;
18 native-action intervals have median 0.129 and maximum 0.692 seconds. The 30 verified response
views have median 7,347 and maximum 17,077 bytes. Retained responses occupy 1,208,406 bytes and
SQLite/WAL 3,620,864 bytes. These are finite observations, not a stability threshold.

The required representative harness qualification is complete. The capability matrix preserves
conditional route limits and later product questions; it does not certify the comprehensive CAOL
feature campaign. No qualification game remains running.

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
