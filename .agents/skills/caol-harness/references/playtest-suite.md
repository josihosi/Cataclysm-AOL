# Faction and camp playtest suite

This is the coverage map and replay index. The [work ledger](../../../../.de67/work-ledger.md)
owns assignments; the registry owns compatible scenario/build selection; run witnesses own proof.
A working recipe is reusable even when a game bug prevents its intended outcome.

## Current coverage and next complete pass

Snapshot: 2026-09-29. No complete suite pass on one final build is established. Current R011
roof-assessment continuation stays with its existing input owner; this page does not restart it.
R034's source repair is built/tested; the saved native continuation reached a corrected assessment,
same-ID return and report at minute9000. The five-member camp denied the raid at minute9060 for
the measured party-power margin. A capable roof assault remains unconfirmed.
Earlier results below retain their original source/build scope, not current-build certification.

When Josef requests the next full pass, record the selected build and pass date here and replace
the replay column as each arm returns: `pass`, `fail` or `inconclusive`, with the exact result link.
Keep `not run` for untouched arms. A declared dependency is not a pass. Update one row; put detailed
chronology in the run report. Do not copy logs or add another registry.

**Next full pass:** not started; build not selected. Finish the current safe continuation first.

| Arm / behavior covered | Reusable route and compatible start | Retained evidence and open boundary | Next-pass result |
| --- | --- | --- | --- |
| 1. Cannibal smoke → scout/report → raid/combat | [Smoke recipe](smoke-raid-replay.md); clean closed-window prepared save, ordinary view. Separate viewer variant below. | [R020 ordinary fatal outcome](../../../../build_logs/first-smoke-020/ordinary-assault-posthit-raid-outcome-result.json), [R027 repaired saved-contact combat](../../../../build_logs/first-smoke-027/r027-min8597-raid-20260928/native-result.json). Continuations do not independently prove fresh admission. | Not run |
| 1v. Cannibal viewer raid | [Dual-clairvoyance variant](smoke-raid-replay.md#clairvoyance-raid-replay), separate fresh closed-window start. | Historical viewer/control evidence stays separate from ordinary exposure proof; explain every living attacker and defender. | Not run |
| 2. Admission → extinguish → routine outings | Shared smoke setup, then [signal-off branch](#signal-off-routine-branch); independent clean start or explicitly scoped admitted-roster continuation. | [R018 empty watch/return/report](../../../../build_logs/first-smoke-018/native-route-watch-return-report-result.json). Populated observations, different POIs and actual haul remain R019 work. | Not run |
| 3. Roof smoke | [Roof variant](smoke-raid-replay.md#roof-variant), actual weather/range and source height. | [R033 measured party margin](../../../../build_logs/first-smoke-033/result.md); [R034 native assessment and report](../../../../build_logs/first-smoke-034/r034-min9061-roof-native-result.md) credited bound smoke, reached certainty5, returned scouts4/5 and delivered report revision1 at minute9000. The five-member camp declined a raid at minute9060 for insufficient party power. A capable assault and independent light channel remain open. | Not run |
| 3c. Combined smoke + light | Exposed roof/open-window compatible geometry; verify both channels, then the faction journey. | Smoke-only success does not pass this arm. Verify independent light range/LOS; branch by faction and report each separately. | Not run |
| 3r. Rural roof; optional dual-faction variant | [Rural roof protocol](rural-roof-replay.md); offline clean pre-signal countryside fixture A, A+bandit fixture B. | Owner-authorized fixture preparation pending; preserve original city arm. Separate both-sense, entry/stairs/combat and bandit payment/fight outcomes. | Not run — fixture preparation |
| 3f. Panicked raider withdrawal | [Rural panic branch](rural-roof-replay.md#panic-and-withdrawal-regression), real raid contact; one induced fleeing member and calm companion. | Planned: escape downstairs, homeward route and return/casualty continuity; roof arrival credited separately. Active bandit Fight control. | Not run |
| 4. Bandit smoke → peaceful payment/debt → departure/return | [Bandit branches](bandit-journeys.md), independent bandit start and preserved pre-choice branch. | Shared controls validated in cannibal play; bandit selectors/timing/outcomes need native validation. Cross-reference CAMP-PAY-RETURN; no blanket bandit pass. | Not run |
| 4r. Bandit admission → signal off → routine outings | [Bandit routine branch](bandit-journeys.md), own faction start. | Shared controls are reusable; bandit-specific travel/observation/cargo/knowledge still needs native proof. | Not run |
| 5. Bandit refusal → effective armed fight | [Bandit branches](bandit-journeys.md), independent pre-choice copy. | Actual shots, damage, targets and casualties required; peaceful branch proves no fighting outcome. | Not run |
| 6. Leave base → loot → return; exposure and bubble handovers | [Away/return variations 6a–6o](away-return-journeys.md), preferably both factions in the validated rural fixture. | Planned: 24h absences at scout/watch/report/response/payment phases, exposed stalking, pursuit knowledge, split pairs, casualties, reload, vertical crossings and burnout/relight. Preserve independent faction outcomes; no native pass yet. | Not run |
| 7. Lamp light only | Independent electrical/lamp save; replay relevant faction branches with light and no smoke. | Awaiting Josef's lamp baseline. A smoking brazier does not substitute. Record cannibal and bandit results separately; other arms can proceed. | Not run — dependency |
| 8. Patrol → intruder defense; peaceful restraint | Prepared two-NPC patrol/locker start; see patrol detail below. | Preserve accepted assignment/locker evidence; actual mobilization/combat and paid-shakedown restraint remain separate outcomes. | Not run |

An explicitly requested full pass includes these arms; historical “no need to repeat” notes do not
excuse an arm from that pass. Historical matrices outside this index remain reference evidence,
not extra mandatory tests. Run sequentially for now. After a fix, first replay its saved regression,
then finish a complete pass on the resulting build; results from an earlier build remain historical
until rerun. Continue independent arms when useful, keeping build boundaries honest. No finite pass
proves absence of every bug: it establishes these journeys under their recorded conditions.

Use the [suite evidence map](playtest-evidence-map.md) to select existing capture and queries, and identify scoped proof gaps before an arm. It covers the away/return subcases too.

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
