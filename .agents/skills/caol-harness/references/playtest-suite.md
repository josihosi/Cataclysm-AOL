# Faction and camp playtest suite

This is the coverage map and replay index. The [work ledger](../../../../.de67/work-ledger.md)
owns assignments; the registry owns compatible scenario/build selection; run witnesses own proof.
A working recipe is reusable even when a game bug prevents its intended outcome.

## Current coverage and next complete pass

Snapshot: 2026-09-30. R048 proves ordinary-view rural roof arrival and combat; R051's
bandit-only journey is preserved at the diagnosed fallback-watch travel mismatch. R037 retains
both typed light/smoke report proof on its bound build. None of these closes a complete suite pass
on one final build; earlier results retain their original scope.

When Josef requests the next full pass, record the selected build and pass date here and replace
the replay column as each arm returns: `pass`, `fail` or `inconclusive`, with the exact result link.
Keep `not run` for untouched arms. A declared dependency is not a pass. Update one row; put detailed
chronology in the run report. Do not copy logs or add another registry.

**Next full pass:** not started; build not selected. Resume R051 after its scoped repair; retain completed R048 proof.

| Arm / behavior covered | Reusable route and compatible start | Retained evidence and open boundary | Next-pass result |
| --- | --- | --- | --- |
| 1. Cannibal smoke → scout/report → raid/combat | [Smoke recipe](smoke-raid-replay.md); clean closed-window prepared save, ordinary view. Separate viewer variant below. | [R020 ordinary fatal outcome](../../../../build_logs/first-smoke-020/ordinary-assault-posthit-raid-outcome-result.json), [R027 repaired saved-contact combat](../../../../build_logs/first-smoke-027/r027-min8597-raid-20260928/native-result.json). Continuations do not independently prove fresh admission. | Not run |
| 1v. Cannibal viewer raid | [Dual-clairvoyance variant](smoke-raid-replay.md#clairvoyance-raid-replay), separate fresh closed-window start. | Historical viewer/control evidence stays separate from ordinary exposure proof; explain every living attacker and defender. | Not run |
| 2. Admission → extinguish → routine outings | Shared smoke setup, then [signal-off branch](#signal-off-routine-branch); independent clean start or explicitly scoped admitted-roster continuation. | [R018 empty watch/return/report](../../../../build_logs/first-smoke-018/native-route-watch-return-report-result.json). Populated observations, different POIs and actual haul remain R019 work. | Not run |
| 3. Roof smoke | [Roof variant](smoke-raid-replay.md#roof-variant), actual weather/range and source height. | [R033 measured party margin](../../../../build_logs/first-smoke-033/result.md); [R034 native assessment and report](../../../../build_logs/first-smoke-034/r034-min9061-roof-native-result.md) credited bound smoke, reached certainty5, returned scouts4/5 and delivered report revision1 at minute9000. The five-member camp declined a raid at minute9060 for insufficient party power. A capable assault and independent light channel remain open. | Not run |
| 3c. Combined smoke + light | Exposed roof/open-window compatible geometry; verify both channels, then the faction journey. | Smoke-only success does not pass this arm. Verify independent light range/LOS; branch by faction and report each separately. | Not run |
| 3r. Rural roof; optional dual-faction variant | [Rural roof protocol](rural-roof-replay.md); offline clean pre-signal countryside fixture A, A+bandit fixture B. | [R048 roof arrival/combat](../../../../build_logs/first-smoke-048/r048-roof-and-route-result.md) proved IDs5/7 on z1; fatal killer unknown. R050 bandit-only derivative is prepared; [R051](../../../../build_logs/first-smoke-051/result.md) reached smoke lead and a separate terrain outing, blocked before departure. Interior stairs/search and bandit demand remain open. | Not run |
| 3f. Panicked raider withdrawal | [Rural panic branch](rural-roof-replay.md#panic-and-withdrawal-regression), real raid contact; one induced fleeing member and calm companion. | Planned: escape downstairs, homeward route and return/casualty continuity; roof arrival credited separately. Active bandit Fight control. | Not run |
| 4. Bandit smoke → peaceful payment/debt → departure/return | [Bandit branches](bandit-journeys.md), independent bandit start and preserved pre-choice branch. | R051 run1fe37273 on40bfb2be proves the clean smoke-led outing/report, once-only gold payment, physical counted custody and both bandits saved home. [Verified segment](bandit-journeys.md#verified-roof-smoke-and-gold-payment-segment). Other signal, receiver, activity and debt branches remain separate; no blanket bandit pass. | Not run |
| 4r. Bandit admission → signal off → routine outings | [Bandit routine branch](bandit-journeys.md), own faction start. | Shared controls are reusable; bandit-specific travel/observation/cargo/knowledge still needs native proof. | Not run |
| 5. Bandit refusal → effective armed fight | [Bandit branches](bandit-journeys.md), independent pre-choice copy. | Actual shots, damage, targets and casualties required; peaceful branch proves no fighting outcome. | Not run |
| 6. Leave base → loot → return; exposure and bubble handovers | [Away/return variations 6a–6o](away-return-journeys.md), preferably both factions in the validated rural fixture. | Planned: 24h absences at scout/watch/report/response/payment phases, exposed stalking, pursuit knowledge, split pairs, casualties, reload, vertical crossings and burnout/relight. Preserve independent faction outcomes; no native pass yet. | Not run |
| 7. Lamp light only | Independent electrical/lamp save; replay relevant faction branches with light and no smoke. | Awaiting Josef's lamp baseline. A smoking brazier does not substitute. Record cannibal and bandit results separately; other arms can proceed. | Not run — dependency |
| 8. Patrol → intruder defense; peaceful restraint | Prepared two-NPC patrol/locker start; see patrol detail below. | Preserve accepted assignment/locker evidence; actual mobilization/combat and paid-shakedown restraint remain separate outcomes. | Not run |

An explicitly requested full pass includes these arms; historical “no need to repeat” notes do not
excuse an arm from that pass. Historical matrices outside this index remain reference evidence,
not extra mandatory tests. Run sequentially for now. For an ordinary scoped fix, validate the
affected behavior from a compatible saved regression; do not automatically start a complete pass.
Accepted first-run proof keeps its source-bound scope. Derive regression checks from retained
native evidence, and repair query/schema/checker errors against those artifacts instead of
replaying successful gameplay. A new native run needs changed behavior or a material unresolved
behavioral question. A complete pass is required when Josef requests that pass, not after every
clerical correction. Continue independent arms when useful, keeping build boundaries honest. No finite pass
proves absence of every bug: it establishes these journeys under their recorded conditions.

Use the [suite evidence map](playtest-evidence-map.md) to select existing capture and queries, and identify scoped proof gaps before an arm. It covers the away/return subcases too.

## Final clean sweep and finite edge-case selection — 2026-10-05

The current owner direction, recorded in WEC and the ledger's `final-clean-suite`, is to finish
current playtests, then run the entire agreed suite on the selected final build. A confirmed game
bug is repaired within its existing authority and restarts the whole final sweep from its first
arm. Prior runs remain accepted historical evidence; they are not passes in the restarted sweep.
A checker, query, capture or setup error is diagnosed separately: repair it against retained
artifacts where possible and resume the affected arm from an honest compatible start. Do not
classify missing evidence as either a game bug or a pass. If recovery cannot establish the arm's
outcome, it remains inconclusive and needs only the missing native proof. A harness change that
invalidates earlier observations requires revisiting the affected evidence, not concealing the gap.
Record results in the existing table and current handoff, not another suite registry.

The following five concrete edge cases refine existing arms; they are not five extra copies of
the same journey or a combinatorial cross-product. Reuse already applicable controls and include
the relevant branch in the final pass. Actual fixture identities and live command bindings belong
in each selected start brief, never guessed from this design table.

| Existing arm / distinct risk | Start and meaningful interruption | End proof and retained source coverage |
| --- | --- | --- |
| 4 / 6g: Pay, cancel, then pay a grouped basket; reload after settlement | Compatible retained pre-choice resident encounter with recorded local coins and distant avatar. Cancel the first basket before commitment, then pay through actual native Trade; reload only after a confirmed retained save. | Cancel leaves goods unchanged; settlement debits the actual local payer and deposits matching real home-stash goods once; reload does not duplicate them or charge the distant avatar. Reuse `stash-conservation/result.md` real grouped coin/charge/save-reload controls and corrected RLE reader. Gameplay payment1000 already accepted; no extra preliminary replay. |
| 4: Robbery arrives during an activity | Compatible pre-demand start, begin a supported resumable activity using ordinary controls. Demand/Trade is the interruption, not an artificial test popup. | Readable demand/Trade, correct input owner, preserved unfinished progress/items under the existing interruption contract. Reuse `activity demand keeps native semantic ownership through Pay trade successor` in bandit_live_world_test.cpp and renderer controls; vary activity only for a distinct uncovered lifecycle, not every activity name. Forced incapacity is not ordinary voluntary waiting. |
| 2 / 4r / 6o: Signal disappears after valid learning | A byte-backed completed assessment/report checkpoint; extinguish through ordinary controls, then ordinary departure or continued observation. Keep pre-watch extinction as a separate existing negative example. | Finite knowledge keeps its original age and source, can inform response while valid, and is not renewed by reads/reload. No invented people, inventory or guaranteed raid. Reuse `stationary scout light and smoke share fresh habitation buckets through reload` and prior report-age proof. Closed-room plume semantics remain an owner choice; this case cannot decide it implicitly. |
| 6k / 6m: Pair crosses bubble boundary and reloads | A retained active two-member journey with actual routes; cross the relevant boundary by ordinary travel, retaining the same actors. Reload at a confirmed save with available bytes. | One simulation owner per living member, no duplicate party, route progress preserved, one report/return application. Reuse `homeward reunion retains exact identities and clocks through reload`, `unloaded homeward route persists across actual actor and world reload` and existing boundary controls. Inspect meaningful before/after transitions, not every step. |
| 3f / 5 / 6l: One member flees or is incapacitated during real combat | Compatible actual contact with named party members; use the already authorized panic branch or naturally observed combat injury. No new debug injury/death setup implied. | Fleeing/capable survivor physically withdraws; incapacitated member is not teleported, falsely returned or resurrected; casualties and damage retain actor attribution. Reuse `paid departure retains each actual survivor flight and forced incapacity` and `active scout flight uses ordinary survival before party return`. One coherent encounter can cover overlapping arm questions. |

The expected outcomes above come from existing contracts. New smoke propagation, visibility
through walls, changed forced-incapacity behavior or additional debug intervention needs separate
owner scope reconciliation. A legitimate capability/night gate is not automatically a defect.
At the first meaningful success or divergence, send the coordinator the outcome, original handle
and remaining uncertainty; finish analysis without withholding established gameplay evidence.

## Select and brief one arm

Use [selection and launch](selection-and-launch.md) for the current registry revision and immutable
build. Historical scenario IDs are lookup seeds, not launch tokens or compatibility guarantees:

- Closed smoke: `cannibal.r_caol_first_smoke_011_ordinary_closed_smoke_assault_r016_clean_recovery`.
- Routine: `cannibal.r_caol_first_smoke_terrain_scout_013`; retained minute8760 save already has
  actors and knowledge, so it cannot prove clean admission by itself.
- Bandit payment: `r032.camp_payment_return_v001_mcw`; recorded locker/ten-coin setup, no injected
  contact. `bandit.live_world_nearby_camp_smoke_mcw` injects fields, not natural ignition.
- Bandit fight: `r032.camp_defense_shakedown_v004_mcw`; verify its actual actor/priority setup.
- Patrol: `r032.camp_defense_zombie_v003_mcw`; preserve its recorded setup and scope.

All arms use the shared [mutation policy and life-support setup](setup-and-interactions.md#mutation-setup-for-suite-runs). Roof and day-away faction runs default to avatar `DEBUG_LS`; ordinary-view and explicit clairvoyance variants stay distinct.

Give Luna one arm, intended outcome, current session/start-save, completed setup, expected next
observable development and remaining proof question. `registry-query --coordinator-brief FILE`
carries that brief. Link the relevant recipe instead of copying the whole suite. Resolve actor/menu
IDs live. Enable supported [NPC decision tracing](evidence-and-diagnostics.md#npc-decisions-during-playtests)
for the relevant actors; disclose unsupported coverage. Inspect meaningful changes, not ritual stops.

A positive raid arm needs a genuinely capable camp. An honest power denial is valid behavior, not
an AI failure or a successful assault. Keep that negative result and select a compatible natural
start, or explicitly record an authorized setup variant. Do not quietly change the roster or margins.

## Signal-off routine branch

Reuse [ignition/wait/actor controls](smoke-raid-replay.md). After natural roster admission,
extinguish via the live Examine choice and verify fire/signal-off; existing remembered knowledge
may persist. Follow same-ID departure → watch or physical visit → return/report, actual cargo and
supply. Observe naturally selected different POIs and knowledge provenance while the player moves.
An empty watch is a valid branch, not haul proof; do not force loot or a target to obtain a pass.
R018's admitted-actor save validates its journey, not fresh ignition/extinguishing.

## Viewer and patrol variants

Arm1 includes a separate [dual-clairvoyance replay](smoke-raid-replay.md#clairvoyance-raid-replay)
when exercising the requested viewer coverage. Ordinary-view combat remains distinct. Reuse the
same controls; record viewing mutations and judge each surviving attacker/defender by actual action.

For patrol/defense, use the owner's next-test priority10 instruction for both actual camp NPCs;
verify duty/shift and completed locker pickup, not just configuration. Observe release from duty,
recruitment and effective intruder combat. Paid peaceful collectors must not provoke unwarranted
defense. Cross-credit only the matching CAMP-DEFENSE/PATROL-SOUND evidence.

## Night alarms and party response

Owner-added 2026-10-02; **not run**. These are independent FIRST-SMOKE duty/rest arms,
not prerequisites for continuing R051's retained return. Use a compatible disposable save and
immutable repaired build. Luna owns native input. Preserve an unmodified start and record exact
night time, faction/camp/operation identity, actual members, positions, sleep/effects and duty.
Use ordinary avatar vision; do not add clairvoyance to the tested NPCs. Apply the shared SafeOff
and player Debug Life Support setup where needed, declaring any combat consequence; mutations
on the player do not substitute for defender perception. Disclose induced sleep/setup separately
from naturally observed bedtime and do not strip forced incapacity to manufacture a result.

**A. Camp patrol alarm at night.** Use two actual assigned eligible camp NPCs with Patrol10 and
valid patrol terrain/orders. Establish an awake response control and an ordinarily sleeping
eligible off-shift defender. Josef permits a recorded zombie spawn for the threat, or combining
this arm with an actual cannibal raid; keep setup distinct from native alarm/wake/defense proof.
Cause a real awake guard to perceive a hostile and raise the existing
patrol alarm; position the sleeper without direct enemy sight where the alarm can reach them.
Verify alarm origin, reception, sleep ending and actual response movement/defense through the
native scheduler. A diagnostic raised alarm may isolate delivery but does not prove natural
triggering. Observe that the responding NPC does not immediately choose bedtime again. After
threat resolution/expiry, normal duty/rest eligibility returns; do not demand immediate sleep.

**B. Attack a bandit camp at night.** Josef wants to babysit this arm: notify him and coordinate
his participation before launch; do not start it unattended. Use a generated bandit home camp with an awake member and
an ordinary sleeper; retain actual generated membership rather than assigning player-patrol
jobs to make it work. Attack or visibly threaten the awake member so real hostility originates
the alert. Include an occluded but reachable companion. Show alert delivery, waking and each
member's actual defense, approach, regrouping or justified flight. Record attack/damage attribution
and legitimate casualty/incapacity; survival is not a requirement. A sleeping member directly hit
only proves injury waking, so it cannot replace the unhit recipient arm.

**Party extension and controls.** At a suitable existing dispatched-bandit checkpoint, provoke a
real fight involving one member while a reachable companion lacks direct sight; test the same
shared rule for a cannibal party on its own bound state. This does not require restarting fire,
watch and report setup. Keep normal peaceful shakedown/payment non-hostile. Focused production
tests cover unreachable/unrelated members, narcosis/forced incapacity, fear/flight, stale membership
and alarm expiry/reload; use native controls when needed to resolve an actual delivery difference.
Never claim that a player-basecamp result proves generated-camp or dispatched-party behavior.

**Evidence and checkpoints.** Capture before trigger, alarm reception/wake, first meaningful
response and resolution/expiry. Retain source/build/run/turn binding, source and recipient IDs,
sleep/incapacity before/after, membership/order, target knowledge, chosen action, movement and
incoming/outgoing damage. Use existing selected-actor traces; missing observation is not proof
of no action. Diagnose the first material mismatch with the actual caller/motor; avoid repeated
unchanged waits. Save safely and retain the failed arm separately. Return each arm as pass, fail
or inconclusive with its exact evidence handle; these recipes currently provide no native pass.

## Bandit waiting and camp-NPC contact

Owner-required native arms for the consolidated [bandit encounter contract](../../../../doc/npc-bandit-encounter-spec.md). Implementation/source tests do not substitute for these playtests. Run with the existing R051 Luna owner after the same R066 implementation supplies an immutable compatible build; preserve existing completed proof and nearest useful saves. No competing live input owner or repeated scout setup is needed.

### Waiting outside local simulation, then activation

Use a valid dispatched shakedown and establish that its destination camp/encounter is actually outside the loaded reality bubble but represented by the overmap operation. An adjacent OMT alone does not prove unloading. Record actual loaded-map bounds/active NPC membership and exact party/operation/generation. Advance meaningful native time across an applicable operation update, save and reload while still outside local simulation. The same visit must remain pending without a demand, payment, synthetic combat, refusal from unloaded time or replacement dispatch. Move normally so the camp becomes locally simulated, then observe that the same members/operation resume physical contact once. Verify a real receiver and demand, and that another update/reload does not duplicate the opening or reservation. Preserve actor identities and actual casualties; do not edit ownership/contact flags to force the result.

### Camp NPC receives demand; player on adjacent OMT

Keep an actual basecamp NPC at the target camp and move the avatar onto an adjacent overmap tile. Verify both OMT positions and actual local simulation coverage. Arrange the meaningful distinction: the bandits communicate with the camp NPC, not merely the nearby avatar (retain actual receiver, sight/hearing and contact evidence). If geometry also permits direct avatar contact, move within the adjacent tile or select a compatible setup that separates the receivers; do not count an ambiguous avatar demand as camp-NPC proof. Observe the early threat and camp-response menu identifying the camp/recipient while the avatar remains on its adjacent OMT. Use a saved branch to Pay with reachable camp goods: demonstrate local goods/cargo/value change, distant avatar inventory unchanged and actual peaceful departure. Verify no repeat payment/menu after update or reload. Retain an independent Fight branch when needed by the existing suite: local response, no teleportation, actual attacks/survival and truthful damage evidence. Camp NPCs must not attack or be attacked solely because a peaceful party is present.

For both arms retain native contact/menu/action and saved-state handles, source/build/run identities and before/after operation state. Report missing contact, remaining unloaded state or a behavioral divergence honestly; neither proximity nor a menu-only screenshot establishes the whole outcome. Setup interventions are preparation, not natural gameplay credit. The existing empty-camp/unheard/source controls remain in the contract; these two requested native arms are explicitly pending until actually observed.

## Encounter variability: simple coverage checklist

Owner-requested comparison, 2026-10-03. “Existing” means an arm is already specified, not that it passed. Use retained starts to combine compatible observations; do not multiply every row by every monster, time and terrain. Mark actual build/run results in the existing arm record. New cases below are pending.

| Encounter variation | Bandit coverage | Cannibal projection / remaining native check |
| --- | --- | --- |
| Player encountered outside | Existing arm4 peaceful demand → Pay → goods/departure; arm5 independent Fight. Early-contact behavior remains pending. | Existing arm1/3r: real hostile arrival and effective attack, no demand. R048 roof proof is historical at its scope. |
| Player inside/on another floor | **Add to early-contact arm:** audible indoor shout opens demand; an actually unheard/occluded receiver does not. | Existing roof/interior/6n route; explicitly finish real entry/stairs/search and engagement, not just arrival on target OMT. |
| Camp NPC receives contact; avatar adjacent OMT | Required arm above: actual NPC receiver, remote response, local Pay and independent Fight. | **Add to arm8/1:** raid contacts actual camp defenders while avatar stays adjacent; observe real targets/damage and defender response, no avatar substitution or Pay menu. |
| Empty camp or no reachable receiver | **Add:** local search ends by existing bounded withdrawal; no invented demand/refusal or current-player tracking. | **Add to6f:** raid searches the known site, has no omniscient pursuit of absent avatar, then follows existing search/withdrawal outcome. Record any missing production rule as a finding, not authority to invent loot or auto-battle. |
| Destination outside reality bubble → load | Required pending-wait/save/reload/once-only activation arm above. | Extend existing6e/6f/6m: verify actual raid ownership/progress while unloaded and same-ID local arrival/combat on return; no duplicate actors or fabricated offscreen casualties. Bandit wait behavior must not be silently imposed on raids. Any missing raid destination rule is an explicit outcome/design gap. |
| Player or camp ally attacks before demand | **Add separate pre-contact branch:** actual attack (including an avoided hit where observed) releases self-defense; continue to effective fight. Distinct from selecting Fight. | Existing combat/night attack and party alarms; real player/defender attack must permit defense, but raid is already hostile. |
| Zombie/other threat interrupts encounter | **Add:** meaningful third-party attack before/around demand; bandits defend/flee appropriately without treating it as player refusal or attacking peaceful camp NPCs. Resume demand if survivors remain eligible. | **Add to existing combat arm:** real third-party interruption with actual target selection/survival and subsequent raid continuation or justified retreat. One representative threat suffices unless code shows a distinct capability matters. |
| Sleep, forced incapacity, night alarm | Existing night arms and party extension, still pending at their stated limits. | Existing separate cannibal party alarm/response arm; night raid schedule and survival stay faction-specific. No demand variant. |
| Fear/flight, separation, casualties | Existing3f,6k/6l: individual retreat, survivor continuity, no resurrected/duplicated member. | Existing3f and6k/6l; verify raid-specific withdrawal; one survivor and all-dead branches may reuse observed outcomes. |
| Reload during interaction or after outcome | Existing required pending/menu/payment controls plus6m/6g; no duplicate demand/payment, peaceful departure remains peaceful. | Existing6m/6l: preserve combat/withdrawal/casualty identity; no Pay/debt tests apply. |
| Player breaks sight and moves elsewhere | Existing6j seen/unseen comparison: reported camp is not live player tracking. | Same6j with raid knowledge/perception; preserve separate faction conclusion unless equivalence below is demonstrated. |

### When one faction can cover a shared path

Current source provides partial reuse, not whole-encounter equivalence: `bandit_live_world::is_active_local_assault_member` admits both raid and combat-released shakedown into `active_local_assault_site_for`; `npc::on_attacked`/`raise_faction_alarm` are common callbacks with faction/group context. However `hostile_operation_player_relationship_for` is shakedown-specific, and `active_hostile_withdrawal_site_for` explicitly filters raid. Shared names therefore do not prove equal entry, eligibility or recovery.

A focused source regression may cover both with parameterized real faction inputs. To avoid a duplicate native segment, retain the actual caller → shared helper → outcome path, relevant input/branch equivalence, source/build binding and matching controls for both faction entries. Then credit only that shared segment (for example an identical motor step), not faction dispatch, nighttime gate, parley, target choice or withdrawal. Record the equivalence beside the result; without it, keep the faction-specific native check. No exhaustive cross-product or blanket “same code” exemption.

## Return and tend the suite

Return build/run/start identity, outcome or first concrete divergence, tested action/query corrections,
evidence links and save/input ownership. Update the affected recipe when real use improves a step;
reuse shared segments and keep unvalidated faction differences explicit. Coordinator updates the
coverage row and current ledger sentence after a material result. No extra review gate, mandatory
save schedule or duplicate protocol database. Sol adds macros only for demonstrated repetition.

Keep the existing performance check (>100ms actual simulation turns), separate wall-time latency,
and harness-only analytical logging. Preserve first failing checkpoints for exact repair replays.
Do not stop merely at a checkpoint, first harm or a future night gate; follow the intended outcome
or establish the concrete contradiction. Use [witness/finish](witness-and-finish.md) to retain
native proof and safely close or hand over the owned session.
