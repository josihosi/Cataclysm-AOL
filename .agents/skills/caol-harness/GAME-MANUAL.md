# C-AOL game manual

This is the skill's reusable game knowledge and assessment manual. Use the section matching the
current question; do not read every reference before playing. The original run logs, saves,
receipts and code remain the evidence. Add a finding here when it changes how a later test
should be run or judged. Do not paste a worker transcript or duplicate a run report.

## Start with a story; reuse proven command batches

Before a first run, provide the scenario story, compatible starting state, intended observable
outcome and relevant known controls. Mark borrowed or untried segments honestly. When actual use reveals a missing reusable control, distill that segment into the owning reference
under [note maintenance](#add-prune-and-correct-notes). Preserve useful CLI syntax and parameters,
not the whole transcript or another copy of an already documented procedure.
Use existing supported wait/move/selection mechanisms; a list of commands is not automatically
an executable batch-file format.

Each reusable segment gives its compatible entry state, exact commands with only live values
parameterized, the existing query that obtains those values, and its useful endpoint query.
Late-bind current session/frame/UID/letter/prompt values; never copy historical authority.
Break at a decision, unexpected prompt, rejection, danger or performance alarm. Preserve pending
request identity and collect it rather than replaying submitted input. Merge adjacent compatible
waits with no intervening decision and stop once the outcome is proved.

The chaperone and actual player's start handoff carries the short canonical
[world vision](SKILL.md#the-world-we-are-testing), followed by the selected build/save/scenario,
current checkpoint and delta, relevant story and command segments, and remaining proof. Link accepted
history and original receipts for retrieval; omit obsolete builds, superseded diagnosis, completed
branches and duplicated manuals from the ordinary start. Keep any historical fact that changes
this run's decision or evidence ceiling. Parent and input helper receive the same current segment.
Repair a mistaken query against retained evidence; do not replay accepted gameplay to manufacture
a recipe. Distill other successful arms from their existing logs as their controls are needed.

Batching must retain performance visibility. Existing collection reads run-bound native turn
telemetry while input is pending and retains alarms and recoveries across internal polls. Examine
its `turn_assessment`, including transient alarms; an unavailable trace or unconfigured assessment
is not a clean performance result. Native turn timing, CPU/RSS and command/transport latency are
different measurements. Preserve scenario-bound thresholds and raw evidence. Report slow turns
and stalls to the coordinator before blindly extending another batch. Collection now returns a
new alarm promptly while retaining the pending request; this returns control to the worker, not
an automatic cancellation of native gameplay. Inspect the evidence and collect that same request.
The existing waiting alarm checks a 100-turn mean against 100 ms; it is not an individual-turn
spike alarm. General spike/slow/stall assessment requires the scenario-owned performance config;
without it, those measurements remain unassessed. Read-only `performance` remains available while
a request is pending. Monitoring must not require splitting every quiet native wait into short
gameplay actions.

## Compose a useful scenario

Start from the behavior to judge and the conditions that let it occur. Reuse the nearest compatible
fixture and proven controls. Fresh full-story tests start before their first required stage;
focused tests can begin at a compatible checkpoint. Combine related questions in one journey
when the setup and observations support both, rather than repeating the whole campaign.

Choose practical setup from [mutations and interactions](references/setup-and-interactions.md).
Avatar teleportation can get the observer away while a fire is still burning; invisibility can
reduce interference while following a party. These are useful experimental tools, not failures
of play. Keep ordinary sight when testing reciprocal detection, natural needs when testing
survival, and actual terrain/actors when testing their route. Never substitute a debug arrival,
report or edited NPC mission for the mechanism whose behavior is the question.

| Question | Establish useful conditions | Follow through to |
| --- | --- | --- |
| Camp progress while the player is away | A real active operation and burning source; teleport or travel away as appropriate, checking actual unloading | Required elapsed time, same-ID progress, return/report and persistence |
| Fire burnout and renewed discovery | Finite fuel, known source zones and no unintended ongoing refueling | Actual fire/signal loss, remembered knowledge, relight and subsequent observations |
| Survivor continuity | Compatible real party; X4's exact controlled casualty and observation setup | Surviving member's actual continuation, casualty persistence and reload |
| Interrupted work | A named NPC performing a real resumable local activity, with visible progress/materials | Actual interruption, supported resumption or attributable refusal, completed work without duplicated goods |
| Natural encounter or pursuit | Real visibility, hostility and reachable geometry appropriate to the arm | Native response, contact/combat/escape and its consequences |

A long wait is useful only when the expected change can occur in the current conditions. If the
fire is being refueled, waiting longer is not a burnout experiment. If sorting already finished,
a later abstract mission story is not an interruption of that activity. Change the irrelevant
setup, preserve the actual observation and continue toward the missing condition. Keep elapsed
time, actor identity and real simulation ownership honest through every shortcut.

## Contents and agent routing

| Current question | Go to |
| --- | --- |
| What did an NPC actually do, and why? | [Assess an observed action](#assess-an-observed-action), then [Evidence and diagnostics](references/evidence-and-diagnostics.md). |
| What should each NPC watch interval check? | [NPC interval check](#npc-interval-check). |
| I am in the standardized base; what can I assume? | [Standardized base orientation](#standardized-base-orientation). |
| What should a realistic loot trip and return check? | [Survival trip assessment](#survival-trip-assessment). |
| How should I move, wait, or handle an interruption? | [Wait and observe](#wait-and-observe), then [Behavior and movement](references/behavior-and-movement.md). |
| How do I show the player more without changing signal provenance? | [Viewing setup](#viewing-setup), then [Setup and interactions](references/setup-and-interactions.md). |
| How do I create or adapt a scenario? | [Selection and launch](references/selection-and-launch.md), its scenario-authoring links below, and [Verified save templates](#verified-save-templates). |
| Why did a prepared save open to a blank screen or Main Menu? | [Check the effective profile](#check-the-effective-profile), then [Selection and launch](references/selection-and-launch.md). |
| A save reload opens the known item-sorting debug dialog and the CLI stays at Loading | [Dismiss the known reload dialog](#dismiss-the-known-reload-dialog). |
| Which save should start a smoke or light arm? | [Verified save templates](#verified-save-templates). |
| Must I recheck a prepared save's entire initial scene? | [Reuse a verified baseline](#reuse-a-verified-baseline). |
| How do I light, extinguish or check a prepared fire? | [Fire in a prepared brazier](#fire-in-a-prepared-brazier), then [Item activation](references/setup-interaction.md). |
| How should the CLI show the game? | [Readable play output](#readable-play-output). |
| Which game facts have already been learned? | [Current reusable findings](#current-reusable-findings). |
| What should I record, revise, or prune? | [Add, prune and correct notes](#add-prune-and-correct-notes). |
| Are detailed logs confined to harness sessions? | [Logging scope](#logging-scope). |

For scenario authoring, reuse the existing registry and a relevant source manifest returned by
its exact scenario query. [CONTROL_LOOKUP](../../../tools/openclaw_harness/CONTROL_LOOKUP.md)
is the current detailed control catalogue. The old interaction matrix was consumed; use the
relevant existing recipe and current native controls rather than that missing dependency.
### Write reusable instructions, not a transcript

The maintained library has 50 distinct scenarios. Keep it useful rather than growing it with each
run: replace a redundant recipe when a better one covers its question, and keep temporary variations
in the current task brief. A lasting addition should cover a genuinely missing behavior; delete its
superseded counterpart instead of archiving it. The catalogue is coverage, not a queue to execute.

Prefer adapting a maintained setup to creating another scenario name. A separate declaration is
useful when a materially different starting state or runtime requirement cannot be represented
sensibly by the existing route. A new run, build, date, bug fix, weather setting, distance or actor
count normally belongs in the current brief or a parameterized comparison in the existing recipe.
Do not expand every combination into a permanent scenario. Before adding one, search the nearest
family and explain the distinct reusable question in its description. This is ordinary authoring
judgment, not a new approval gate or a requirement to run the whole library. Start from the prepared open
or closed house when it saves work, then adapt the surrounding world to the question.

Describe the question, relevant starting facts, permitted setup, useful observations and meaningful
endpoint. Separate essential conditions from illustrative distances, order and commands. Let the
player choose actions and supported batches from current controls, recover from ordinary prompts,
and change an unhelpful tactic. A recipe is not a mandatory command queue or a promise of success.

When copying a declaration, replace obsolete descriptions, family labels, source claims, objectives,
steps and proof targets together. Keep only relevant constraints. Check the result against the
actual save and brief, rather than merely making lint pass. Never lower gameplay-evidence eligibility
to silence a validation error when gameplay evidence is the task. `setup_only_debug` currently
classifies the whole run as setup support; it does not mean “this gameplay test also contains setup.”
Use the supported focused reference route for genuine downstream observation; setup itself earns
no behavioral proof. Preserve actual source identity and permissions, not another entry's name as
an admission shortcut.

For interactive play, bootstrap describes the minimum current input boundary, not every control
that might be used later. The maintained reference route requires `world.wait`; movement and other
controls are discovered from the current surface. Do not invent a generic `world.move` action or
copy a long readiness list from an unrelated test. Validate the declaration and its actual registry
selection/evidence class before launching; a valid JSON file alone does not establish a useful run.
Do not alter a live selected manifest beneath its session.

### Carry forward useful learning and remove residue

After a useful playtest, update the existing recipe with the preparation that actually worked,
relevant controls and log queries, meaningful outcome and limits. Include only facts that change
the next player's decisions. Parameterize live IDs and explain how to retrieve them. Replace wrong
instructions; do not append a contradictory success note or preserve a transcript as a procedure.
A successful setup can remain useful when the test exposes a gameplay bug. A prior pass is scoped
evidence, not assurance that another build or condition passes.

Keep reusable starting saves, recipes and current necessary working state. Once a run is settled,
close its game/helpers, verify exit and delete generated saves/logs/captures and obsolete declarations
through the existing cleanup/registry routes. Preserve owner originals, current continuations and
concise commit/run/findings in task context. Do not create archives or a second historical backlog.
Delete superseded failed setup variants after incorporating their useful correction into the
maintained route. Failure alone does not make a still-useful reproducer obsolete. If a temporary
declaration is genuinely needed, keep its purpose and removal condition in the current task;
remove it through the supported registry cleanup route once that need ends. A saved checkpoint
or successful run does not automatically deserve another permanent entry. After adapting a recipe,
check its public catalogue description still matches its save, controls, question and evidence
limits. Leave live selected declarations unchanged until their run settles.

The [skill entry point](SKILL.md) routes launch, live controls, evidence, setup and closeout.
Only Luna operates live sessions. Sol can analyze source, logs and saved data or repair code.

## Adapt the surrounding overmap

For a missing camp, the native Overmap Editor can place a special in the disposable world.
Use `world.debug_menu`, discover its current menu choices, then the advertised overmap placement
controls (`overmap.place_special`, cursor/level controls and close when offered). Resolve actual
special and position parameters from `controls`; menu IDs and availability are live facts. This
is preparation, not evidence that normal staffing, perception or dispatch occurred.

Use the existing fixture setup transforms in
[startup_harness.py](../../../tools/openclaw_harness/startup_harness.py), applied to a disposable
working save before native play through `apply_fixture_save_transforms` / fixture `save_transforms`:

- `seed_overmap_special_near_player` places a special footprint relative to the saved player.
  Fields are `player_save`, `special_id`, `source_special_id`, `site_index` and `offset_omt`.
  It translates an existing source placement; it is not a generic creator for an absent special.
- `overmap_terrain_id_at_abs_omt` edits surrounding OMT terrain. Read its validator/function
  for exact fields rather than improvising save JSON or confusing OMTs with local squares.

The [reference-camp route test](../../../tools/openclaw_harness/r_caol_reference_camp_route_test.py)
contains a concrete `test_nearby_footprint_only_and_real_launch_binding` example using the open
standard save, placing a nearby `bandit_cabin` footprint while preserving the house save.
The existing [open declaration](../../../tools/openclaw_harness/scenarios/caol.reference_camp_open_focused_mcw.json)
and [closed declaration](../../../tools/openclaw_harness/scenarios/caol.reference_camp_closed_focused_mcw.json)
identify the standard sources. Inspect their actual selected scope before adapting them.

Placement is setup, not evidence of a staffed/awake observer or a dispatch. Let the ordinary game
initialize the site and establish the actual roster/opportunity. Choose placement to answer the
visibility or interaction question while keeping the prepared house, windows, fire and player
setup useful. Do not inject received signals, discoveries or outcomes. No offline edits to an
active game or owner originals; changing a bound manifest requires its owner's settled handoff.

## Standardized base orientation

### Josef's two-brazier base save

Choose the start that exposes the behavior:

- For ground-floor firelight, use open windows at night and check reception along the actual sight line.
- For smoke alone, use closed windows and check the escaping plume in daylight. Distant fresh smoke detection stops at night. To measure concealment or weather, keep the source and observer comparable.
- For a scout or shakedown, keep the prepared player and fire. Adapt the surroundings with a staffed camp and usable route. Observe real discovery, investigation, return and the requested interaction. Ignition establishes setup. It cannot establish detection, a demand or payment.

A compatible continuation works for a focused observation. For a different range or workload, record the actual distance or active population. The [overmap setup tools](#adapt-the-surrounding-overmap) can adjust the scene before play while preserving the owner’s original save.

Josef describes the basecamp NPC priorities as 5 across the board; inspect
the selected save's actual roster/settings when they matter to the test. Several wood stacks can
exist while only one is covered by a firewood zone.

The closed-window
archive is the basic unlit start; progressed raid saves carry their own fire, knowledge and
actor history. Match the selected save identity and current World before applying this map.
The normalized house spans `[3148,3440,0]` through `[3163,3455,0]`. Coordinates below describe
that geometry, not every procedural house. After play begins, old positions and fields are
history; check the current player, hazards and pending input before walking.

| Place | Native tile | Use |
| --- | --- | --- |
| Ground-floor brazier | `[3159,3449,0]` | Indoor fire; closed windows make outside discovery smoke-only. |
| Verified downstairs working tile | `[3158,3449,0]` | Brazier is East from here. |
| Roof ladder, ground endpoint | `[3149,3454,0]` | Walk here to ascend, rather than using a staircase that leads down. |
| Roof ladder, upper endpoint | `[3149,3454,1]` | Confirm this tile after `world.level_up`. |
| Roof working/watch tile | `[3155,3448,1]` | Roof brazier is Northeast from here. |
| Roof brazier | `[3156,3447,1]` | Inspect actual fuel and fire before ignition. |

### Normalized firewood zones and stock

The canonical [r050 bandit-only INITIAL](../../../tools/openclaw_harness/fixtures/saves/live-debug/r050_rural_roof_bandit_only_v1/save/TestSetup00) has these saved zones in [Test00's zone file](../../../tools/openclaw_harness/fixtures/saves/live-debug/r050_rural_roof_bandit_only_v1/save/TestSetup00/%23VGVzdDAw.zones.json). These are starting facts, not later run revisions. The indoor-cannibal normalized recipe has identical zone bytes and the same relevant six tiles, even though its complete map packs and faction sites differ.

| Saved identity/name | Type and owner | Enabled; start = end | Starting contents/role |
| --- | --- | --- | --- |
| `zone-11`, Basecamp fire source | `SOURCE_FIREWOOD`, `your_followers` | Yes; `[3159,3449,0]` | Empty unlit indoor brazier. A fuel source zone at the fire is not a separate destination command. |
| `zone-13`, Basecamp kindling | `SOURCE_FIREWOOD`, `your_followers` | Yes; `[3160,3449,0]` | `splinter` kindling on ordinary floor; also a fuel source. |

Both are ordinary nonpersonal, nonvehicle zones, with identical start/end z=0 and no temporary disabling. Saved `2x4` wood is at `[3159,3448,0]` and `[3157,3449,0]`; `[3158,3449,0]` mixes `2x4` with lighters, whistles and flashlights. The roof brazier `[3156,3447,1]` is also empty and unlit. No firewood trap is present on these six inspected tiles. Neither listed zone is a roof-level source; inspect actual roof fuel/access if maintaining the roof fire. This extraction establishes item types and geometry, not the quantity or duration of usable fuel in an altered run.

[Native refuelling](../../../src/activity_item_handling.cpp) (`find_refuel_spot_zone`, `try_fuel_fire`) unions enabled matching-faction `SOURCE_FIREWOOD` tiles within `PICKUP_RANGE=6` of the actor, sorts them by distance, and chooses the first tile with items, accessible contents and a clear pickup path. Only then does it test that selected tile's nonliquid items for burnability. It does not try a second zone when the selected tile has no useful burnable item; the firewood-trap fallback is used only when no zone tile qualifies. The fire destination is an accessible existing fire selected separately (or an explicit fire target). There is no distinct `FIREWOOD_SOURCE` zone type in this recipe. Zone Manager's Source label, a kindling label, a loot-storage zone and the burning destination are different roles.

Stock and proximity alone do not start automatic tending. [Player activity handling](../../../src/player_activity.cpp)
calls `try_fuel_fire` for an active activity with `refuel_fires`; ordinary `ACT_WAIT` enables this in
[player activities](../../../data/json/player_activities.json). Inspect the actual activity after
starting a wait or recovering an interruption. A short step, a look or merely standing nearby is
not proof of an active tending interval. The caller can stop attempting when no suitable fire/supply
is found; after correcting setup, verify the resumed activity and actual behavior.

Use the [sustained-source recipe](references/smoke-light-visibility.md#keep-the-source-alive-through-the-useful-scan)
to distinguish fuel already on the fire from fuel transferred out of the stock zone. Tending need
not transfer an item immediately when the existing fire already has enough fuel. Count actual
pieces/charges, not serialized item entries; retain exact quantities only when they affect the finding.

Widen a source across the **whole accessible wood pile** through the current native zone controls when useful; no one-tile restriction applies. Inspect both sources, stock, actor/fire positions and access before attributing a maintenance failure. Mixed tools or payment goods can be selected and burned; keep the intended fuel supply explicit without inventing a universal ban on brazier or tool coordinates. Fire expiry plus an unassessed report does not establish that one zone caused the failed assessment. The closed Fight run's full initial stock and selected refuel tile were not retained, so that causal conclusion is withdrawn.

Verified native Zone Manager route for optional source expansion:

1. Open `world.zone_manager` from World and select the existing `SOURCE_FIREWOOD` zone by its current advertised target. In the Indoor baseline this was saved zone 11, initially covering the brazier at `[3159,3449,0]`.
2. Edit its start/end bounds with the current native cursor controls to include the plank pile at `[3157,3449,0]` while retaining the brazier. Inspect the resulting bounds, then accept the current save confirmation with **YES**.
3. Confirm the change took effect. The baseline recorded a changed zone revision and subsequent native messages dropping planks/splinters into the brazier. A revision number alone does not prove refuelling.

Historical request IDs, JSON payloads, cursor steps and literal final bounds were not retained; do not invent them or reuse a guessed target. Retrieve current controls and bounds for the new session. This is optional automatic refuelling setup: ordinary manual fuelling also works and zone expansion is not a prerequisite for the raid.

### Walk upstairs and approach the roof fire

From the current ground-floor tile, inspect native terrain and take a passable local route to
`[3149,3454,0]`. The recorded successful route does not supply a fixed downstairs walk from
an arbitrary starting position. At the ladder use `world.level_up`, then check the player is
on `[3149,3454,1]`. A down staircase elsewhere is not the roof ladder.

From that roof endpoint, choose a passable route to `[3155,3448,1]`, southwest of the
brazier. `world.level_down` needs an actual downward connection; shingles at the working tile
are not the ladder. Use current terrain and hazards rather than replaying historical footsteps.
The successful R040/R051 routes establish this geometry at their saved source, not current access.

### Light, maintain or extinguish the fire

Inspect the intended tile with the [World Look Cursor](#fire-in-a-prepared-brazier), rather
than searching Inventory for the furniture. Check actual wood/fuel and a charged lighter.
For an unlit brazier, use the lighter's current **Activate** menu target, then the current
**Light where?** direction: East from the downstairs working tile, Northeast from the roof
working tile. Read any firewood-source prompt and confirm the prepared fuel. Verify native
`fd_fire` at the actual brazier. Historical menu IDs are not reusable controls. The
[fire procedure](#fire-in-a-prepared-brazier) retains the verified lighter and extinguishing
receipts, current control pattern and residual-smoke checks.

For a maintained fire, inspect accessible fuel at the actual fire level and both sources using
[the normalized zone facts and native selection rules](#normalized-firewood-zones-and-stock).
The pickup search radius is six tiles from the actor; a ground-floor source does not certify
roof coverage or actual clear access. Maintenance may burn nonwood fuel. Whole-pile source
expansion is permitted when geometry requires it. Keep the intended fuel and the bulky-payment
basket distinct, and verify actual fire/refuelling rather than infer it from visible stock.
Fire presence alone does not prove exterior smoke or light reach; use
[signal visibility](references/smoke-light-visibility.md) when that is the question.

For bulky M240 setup, step away from the actual brazier before spawning once. Inspect actual
placement and root count; native spill onto adjacent tiles is valid. Do not assume a fixed
destination or spawn again because the pile is spread out. After ignition, retreat over currently
safe passable tiles. Roof-level creature absence does not establish that ground attackers are
passive. Use the actual ladder tile and available door route when deciding how to meet or escape
them; historical coordinates alone do not establish current access.

To extinguish, stand adjacent and open the brazier's native Examine menu, choose the current
enabled **Extinguish fire** entry, then verify `fd_fire` is absent. Smoke and heat may remain;
check their decay before calling the signal off. Do not replace this with an Inventory search.

### Wait, save and return

Use [wait and observe](#wait-and-observe) for the intended next stage and interruption mode.
Across interruptions, keep the accepted request identity and collect its result rather than
replaying the wait. Performance alarms are separate from reporting cost: retrieve the exact
measurement handle before choosing further diagnosis.
Check actual game time, relevant actor IDs and meaningful changes; repeated unchanged motion
or a rejected route needs inspection. For long disposable roof observations, verify
`DEBUG_LS` is active as survival setup; do not toggle it off. Keep ordinary vision for ordinary
detection proof.

Use [Save a checkpoint](references/live-operation.md#save-a-checkpoint) without a movement
step. A skipped quicksave is not a fresh write: verify the saved turn. On returning from a
trip, check current doors, both braziers, remaining fuel and actual signals. For loading errors,
use [the known recovery](#dismiss-the-known-reload-dialog).

## Assess an observed action

Start with the behavior being tested, not its status label. The coordinator's short charter
should identify the current stage, expected observable next outcome, important actors and the
relevant section here. For a committed raid, progress means approach through the terrain and
pursuit/attack of valid occupants; “committed_contact” alone is only a phase. Scout concealment
is a different stage. Give workers a trajectory and evidence boundary, not an action script.

Use `play_cli evidence` or the bound log query for exact run/actor/turn facts; `controls` gives
source handles and available queries. Follow the response's full-evidence handle when its compact
summary omits a needed fact. The [evidence reference](references/evidence-and-diagnostics.md)
explains retrieval and [live operation](references/live-operation.md) explains current owners,
menus and pending requests. A missing field is unknown, not proof that nothing happened.

After Pay, disappearance of the committed-contact projection is normal. Follow the current
paid party and surface facts. When a custody or departure question remains, retrieve the
specific saved member, custody or release facts needed; do not reconstruct the full journal
merely to certify an already accepted positive outcome.

## NPC interval check

At a meaningful transition or when progress becomes doubtful, compare a relevant actor's
previous and current native turn-bound tile/action with the expected outcome. Use local tiles
for nearby entry/combat; an OMT cannot distinguish inside from outside a house. For a raid,
retain the initial ally/attacker IDs so a missing member is noticed. Retrieve terrain/door,
mission/goal/path, gate inputs, health or saved roster when needed to explain the divergence.
A saved-state claim requires a verified fresh saved turn. Do not dump every field on every tick.

For an assigned camp worker, select the current World's advertised `world.inspect_camp_npc`
action with its current `character:ID` target, then use `npc_inspection.close` on that inspection.
The native [inspection producer](../../../src/npc_inspection.cpp) supplies actor ID, calendar turn,
body-part HP, orders, assigned camp, Patrol priority and cached shift diagnostics. Follow the returned
payload or full-evidence handle when needed. Missing effects or unavailable shift cache are not
evidence of no effects or completed duty; these diagnostics are not avatar knowledge.

Dialogue **Examine status** opens the separate native `BODYGRAPH` view. Use that concrete owner's
advertised `bodygraph.close`; closing a nested graph returns to its parent, whose current controls
then apply. The [native display](../../../src/bodygraph.cpp) retains its owner across timeout polls.
The 5949 native adoption verified Close back to World and same-save reload; the old83b missing-Close
finding is historical. Use the bound build's actual controls and supported recovery rather than
inventing a key or sending an action from another owner.

Repeated two-tile repositioning without assault, a hold-off gate during committed attack, or a
missing roster member changes the next action from more waiting to targeted inspection. Preserve
the scene and report the observed divergence, its original handles and the unresolved question
to the coordinator. It preserves the unfinished outcome and routes the repair. A progressing
actor may revisit a tile while fighting or avoiding an obstacle; a justified retreat is not a
stall. Compare actions and state changes, not a position pair alone.

Screenshots can reveal where to investigate; native actions/saves/logs establish movement,
combat and identity. If the needed fact is unavailable, name the exact missing observation and
improve its existing retrieval/trace route rather than adding repeated warnings or fabricating it.

## Survival trip assessment

Use a separate disposable save for a travel arm. Record the initial save identity, intended
signal, player setup and relevant NPC IDs before leaving. A long stationary no-fire control
must stay unlit and unmoved; travel would change its question.

For a movement or loot-trip arm, verify Safe mode **Off** after loading. Use recorded
`DEBUG_LS` and, when that arm permits altered visibility, `DEBUG_CLOAK`; keep an ordinary
visibility arm separate. Clear incidental blocking zombies under the shared
[incidental-danger setup rule](references/setup-and-interactions.md#incidental-danger),
then continue the same run rather than abandoning the route or restarting.
Locate the actual doors in the north-center part of this house from current native
terrain and coordinates; do not aim a walk into a wall or closed curtain. Pass through
the doors by checked local steps, use overmap travel for distance, and return to local
steps near the building when overmap travel cannot choose the entrance. After a rejected
move, inspect the tile and player position before the next action. Repeating a failed
wall/furniture move is a route diagnosis, not progress. Record the successful correction
here when a worker finds one.

At departure, while away in each relevant campaign phase, and after returning, compare the
actual saved player OMT and local tile, carried loot, health, hunger, thirst, sleep and active
debug traits. Follow scouts, raiders and camp allies by stable actor ID even when outside player
sight. Check each actor's tile, mission or job, target/action if exposed, health and roster
presence. Check camp site knowledge, report source, outing members, route and dispatch against
what the player and signals actually exposed. A phase label alone cannot establish pursuit or
combat; a missing actor requires a fresh roster and death/corpse inquiry.

For the prepared two-brazier base, inspect fuel, fire and smoke at the indoor and roof tiles
before departure and on return. If the intended signal went out, use the ordinary lighter
interaction from [Light the brazier](#light-the-brazier), then verify the native fire/smoke and
saved turn. Already burning needs no ignition. Do not project the departure fire state onto
the return scene. A proposed auto-walk/relight macro must check the current tile, fuel, fire,
danger and targeting before input; stop on an unexpected scene.

`DEBUG_LS` holds hunger, thirst and sleepiness stable; `DEBUG_CLOAK` is a separate invisibility
trait in `data/json/mutations/debug.json`. Record them separately because invisibility can
change hostile sight. Melee skill or an axe can be recorded survival setup. Debug HP-to-zero
on incidental travel zombies is setup and cannot prove native combat. For an ordinary needs-survival test, leave life support off from the declared start and provide
real supplies; a debug-aided interval does not test food or water management. Preserve encounters, damage and
interruptions; use the wait mode that matches the fact being observed.

Use existing run-bound descriptors, exact event handles and fresh saves first. If an actor
action or attacker is missing, request one harness-enabled trace scoped to the IDs and turns
that decide the question. Avoid normal-game verbose logs and per-turn full-state dumps. At
closeout, keep only the raw evidence or continuation data needed for a concrete unresolved question;
record that need in current task context. Once settled, retain concise source/run/findings and
reproduction notes and delete generated residue, as described in [Logging scope](#logging-scope).

## Smoke and light visibility

For brazier range formulas, fire growth, sampling/dispatch timing, terrain and cross-level
visibility, use [Smoke and light over time](references/smoke-light-visibility.md). An early weak
packet is not the mature fire's maximum; compare changed samples before ending a visibility test.

## Wait and observe

Choose the existing wait mode for the observation you need. These short CLI examples use a
five-minute interval only as an illustration; use the interval relevant to the test.

Merge adjacent waits with the same interruption policy when no action, observation or decision is
needed between them: `play wait 1h safe` twice becomes `play wait 2h safe`. Likewise, use one
advertised three- or six-hour duration instead of several shorter waits when only the endpoint
matters. The native duration must be available in this scene (hour choices require a watch);
use the largest supported duration that fits the intended interval if the exact total is not offered.
The short CLI maps `2h` to native `wait.2h`; it does not invent arbitrary menu durations.
A longer wait still uses the chosen interruption policy. Collect a pending request; do not issue
another wait while it is running. After an interruption, use actual elapsed time to choose the
remaining interval rather than restart the full duration. Do not merge across a required fuel
intervention, decision, signal-on/off timing measurement, or unresolved prompt. Safe mode here
protects the player; it does not promise detection of every distant NPC event.

Read the resulting frame and retained event evidence at the next useful decision. No automatic
look/save/roster sweep between quiet chunks, and no further wait once the requested outcome is
established. Use the existing save/finish route at that point. Proven ordinary batching needs no
separate trial run; new harness execution behavior needs its own affected controls.

- `play wait 5m safe` (`handle_classified_non_dangerous`) handles recognized harmless
  interruptions and returns control on native near-hostile, pain or attack warnings, actual
  damage, unknown safety or unavailable recovery. Use it when you need to inspect the first
  attack, health change or other consequential event. Earlier R013 companion speech stopped a
  wait before the typed-distraction repair; preserve that old run as history, not current policy.
  A direct `play act wait.5m` chooses the native duration and bypasses this safe-wait handler.
- At a known harmless `Stop waiting?` prompt, select its current `IGNORE` option (`play ignore`,
  or native `I` through the verified input route) when repeated chatter should stop. On this
  native activity prompt, `NO` continues only this time; `IGNORE` continues and remembers that
  distraction **type** for the current activity and its backlog. Reobserve the surface and
  current state before continuing. Assess a new proximity threat or damage prompt separately.
- `play wait 5m stop` (`stop_on_interruption`) deliberately stops at each interruption when
  every one matters to the observation. It stops easily during routine waits.
- `play wait 5m ignore` (`ignore_danger_and_interruptions`) continues through supported
  danger and damage prompts. Use it deliberately when the test should observe combat unfolding,
  including a possible death. Bare `play wait 5m` currently selects this permissive mode; spell
  out the mode so the intended observation is clear.

For discrete turn actions, the [R043 counted-action pilot](references/smoke-raid-replay.md#counted-actions-after-the-r043-pilot)
shows the supported exact-request repeat and its limits. It does not replace these duration waits
for passive raid observation; Tab is native autoattack and can strike a reachable hostile.

A cannibal raid attacking and killing the player or camp occupants can be the expected successful
outcome. Do not classify death itself as a game bug, harness bug or failed raid test. Compare the
actual actors/actions/outcome with the test goal. Retain attacker/target identities and combat,
damage and terminal evidence where available; unknown attribution remains unknown, not proof of
a different attacker or a reason to change raid behavior. A terminal death surface may end the
wait recipe before its requested duration: inspect that surface and the native evidence before
interpreting a generic macro error as a product failure.

An interruption can leave a native picker open while the original wait request is still pending.
Resolve the actual current picker through its advertised controls, then collect the original
request. Do not launch a second collector or assume that a delivered key completed the activity.

Record starting and ending turns, interruptions and the observed outcome. On an inspection stop,
assess the relevant health and NPC state, then choose a continuation from current live controls;
resume only the remaining interval instead of replaying elapsed time. Collect a pending request
before issuing another input. Use meaningful quicksave/actor checks while the character is alive;
a completed death outcome does not require an impossible post-death quicksave.

After a signal creates a real roster and the fire is extinguished, a short home read of
`no_signal_source` says only that the signal is gone. It does not settle later terrain scouting.
In the R013 closed-window save, the source-backed terrain scan runs on 60-minute boundaries and
advances one of 12 nearby offsets per pass; the saved minute-7775 home read preceded the next
scan at minute 7800. Keep one safe owned session through the next relevant scan and compare the
same actor IDs, lead source, outing, return and report at fresh saved turns. Use shorter checks
around a real transition or danger, rather than relaunching after each quiet five-minute read.

Retrieve existing run-bound evidence before adding logging. Any missing diagnostic combat or
actor detail should use the existing harness-enabled capture and exact retrieval route, not
unconditional normal-game logs or repeated per-turn state dumps. Ordinary gameplay must not incur
verbose playtest tracing; a proposed capture change needs a harness-off counterexample as well as
proof that the selected harness run captures the needed fact.

## Viewing setup

Only when expanded viewing is useful, a separate disposable viewing run can use both `DEBUG_CLAIRVOYANCE_PLUS` (Debug Clairvoyance) and
`DEBUG_CLAIRVOYANCE` (Debug Clairvoyance Super). From the current World input owner, open the
advertised debug menu, choose its mutation route, filter for `clairvoyance`, inspect the current
two named entries and choose each through its fresh stable target. Close the menu, pass one real
native turn with `world.pause` or another advertised turn action, and verify both mutation IDs
and the expanded view. Do not reuse historical menu entry IDs. Record each existing or added
mutation and the before/after turn. The mutations are setup, not evidence that the fire or smoke
signal occurred. `src/do_turn.cpp::live_bandit_make_gate_input` also reads the
player's view, so clairvoyance can change local exposure and hostile hold-off behavior. Preserve
a clean no-clairvoyance copy for a concealed-player assault claim. Compare exact gate logs; being
indoors or in darkness does not by itself prove lack of exposure.

## Verified save templates

| Template | Verified contents | Use and limits |
| --- | --- | --- |
| `.userdata/reference-saves/archives/josef-basecamp-closed-windows-20260924-122642.zip` (SHA-256 `4b11469931507c49f0b373934730fed6b729b7a29b87c6f78eca0f728e8320c3`) | 45 save files; TestCamp01 at OMT `[131,143,0]`, closed windows, unlit indoor brazier `[3159,3449,0]` and roof brazier `[3156,3447,1]`; saved NPC2 Tilda has `ACT_MOVE_LOOT`, NPC3 Casey has `ACT_NULL`. No cannibal camp site is present. | Copy the archive into a disposable profile. The indoor z=0 brazier is smoke-only for outside discovery with windows closed; use verified roof or separate open-window geometry for exterior light. The archive is a baseline, not proof of hostile site eligibility or stocked player inventory. Inspect native fuel, lighter, roster and window state before action. |
| `.userdata/reference-saves/josef-basecamp-open-windows-20260924-122356/` | Owner-created corrected open-window reference. Its README says all building windows are open and the source save is unchanged; Josef also describes this comparison setup as having the building doors open. | For the later open-versus-closed signal runs, copy this save separately and verify actual windows, doors, signal source, camp setup and file identity before play. The README's window claim is owner-confirmed, not a native replay or proof that every door is open. |

These standard references are starting scenes, not the current state of any open game. Read the run's latest
save and native World before using a template; a stale instruction such as “fire not yet lit”
does not override a burning brazier in the selected session.

## Reuse a verified baseline

When a disposable save is a byte-exact copy with a retained manifest, compare its file set and
full hashes with that manifest, then verify the launched userdir, World, saved turn and source
binding. Those checks transfer the recorded initial map, NPC and inventory facts to this copy;
do not repeat a full initial-state audit merely because a new worker or game process attached.

Check mutable live facts once on entry: current safe mode, debug mutations, pending input and
actual World/turn. After a time-advancing action, use native metadata and fresh saved turns to
follow each relevant NPC by ID, including unseen actors. A changed save hash, uncertain copy,
reload error or contradiction is a reason to inspect the affected fact again. Keep baseline,
current state and planned setup separate.

## Check the effective profile

`--profile` takes a name under `.userdata`, not a filesystem path. Pass
`first-smoke-010-no-clairvoyance-checkpoint-20260926`, for example, without a directory prefix.
Before game input, verify the resolved userdir has the intended `save/TestSetup00` files and
expected save hashes, then require a same-run native World descriptor. A live PID or startup
`ok` alone does not establish that the intended world loaded.

A sanitized absolute profile path previously launched an empty userdir and Main Menu instead
of the prepared world. Check the effective profile on that specific failure; it is setup evidence,
not a gameplay defect.

## Dismiss the known reload dialog

Use the [debug error catalogue](references/debug-errors.md) for known and new dismissible
warnings: record/report, Ignore, reobserve and continue. Do not wait for coordinator triage.
A different debug message is another record to assess, not a new approval gate.

Startup can remain at Loading with no World descriptor while a dialog is open. Current CLI
startup diagnostics can expose the dialog; use its advertised recovery action when available,
or the verified Peekaboo route described in the catalogue. A live PID or delivered key alone
does not establish recovery. Preserve the log/save and check the resulting native state.

Require a same-run native World descriptor and correct userdir/save identity before gameplay
input. A successful key delivery or visible map alone is not that proof. If the bridge has
already exited, preserve its run and use a fresh source-bound session after closing the owned
game. Do not clean the owner's save or suppress the log merely to proceed with this playtest.

## Fire in a prepared brazier

Check the actual brazier tile through the **World Look Cursor**: choose the advertised
`world.look` action, move its cursor one tile east from the verified adjacent
`[3158,3449,0]` tile with `cursor.east`, then `cursor.confirm` on the brazier at
`[3159,3449,0]`. The returned World frame exposes that tile's native furniture and field
data. `f_brazier` identifies the furniture; `fd_fire` means it is burning. `fd_smoke` and
`fd_hot_air1` show smoke and heat, which can remain briefly after the fire goes out. This
Look Cursor action differs from the read-only `play look` command, which may omit local tile
fields. Inventory lists carried items and cannot tell whether the brazier is burning. Record
the game turn of each check. Avoid another ignition when `fd_fire` is already present.

From the verified adjacent tile in this saved scene, the working CLI actions were:

```text
play act world.look
play act cursor.east
play act cursor.confirm
```

Read each returned surface before the next choice. If the player is elsewhere, use the current
cursor directions instead of copying `cursor.east`. A fresh saved map and the exact tile's field
record are another check when the Look Cursor does not include local tile facts.

### Light the brazier

In the prepared disposable arm, inspect the actual brazier tile, wood/fuel, charged lighter and
window/roof geometry. From an adjacent safe tile, open the current inventory/item-use owner,
select the lighter's **Activate** action, then at **Light where?** choose the brazier with the
advertised direction target. Verify `f_brazier` and native `fd_fire` on that tile, and the matching
ignition result; `fd_hot_air1` alone is insufficient. Retreat about two tiles without stepping
on fire. If it is already burning, observe instead of igniting again. Investigate a concrete
failure before changing supplies; do not substitute a smokebomb. The
[item-activation reference](references/setup-interaction.md) explains current controls; the old
interaction matrix was consumed.

**Closed-window ground floor is smoke-only for outside discovery.** In every scenario copied
from the normalized closed-window save, a fire inside the house at z=0 does not expose light
outside. Do not call its `fd_fire` a valid exterior light packet or switch to a night-light
conclusion merely because it burns longer. Keep the smoke source burning and advance native
time toward the next useful observation. For a smoke-onset/range probe, inspect short intervals
while the signal question is unresolved. For a known working end-to-end recipe, use longer
interruptible waits and retrieve signal/admission/member evidence together at the useful endpoint;
do not make the probe's per-wait checks mandatory for every journey. Stop the narrow probe at
roster initiation or a specific observed **smoke** eligibility failure; other arms continue to their outcome. A fire-only `no_in_range_production_signal`
does not settle the smoke test: the source adapter adds `fd_smoke` to strength and gives a
persistence bonus, which can increase the projected range after smoke appears. In R013 a
fire-only cap of 6 OMT was one tile short of the target; continue or resume from the saved
burning-fire turn until actual smoke is sampled before judging that range gate. An open-window
or rooftop light test needs its own verified geometry and separate disposable run.

For the normalized downstairs working tile, the brazier is East. Use the current lighter
and Activate targets; their numeric IDs change between menus and builds.

```text
play act world.inventory
play act inventory.select --target CURRENT_LIGHTER_ID
play act inventory.item_menu.choose --target CURRENT_ACTIVATE_ID
play act direction.choose --target east
play act prompt.choose --target CURRENT_YES_ID
```

Stop after each line to read the new native surface. `east` applies only when the brazier is
one tile east; select the current direction otherwise.

When the test asks whether a brief fire admits a cannibal roster, advance at least one real
in-game minute after ignition before judging it. At the new native turn, check the exact
signal source, weather and projected reach, then inspect camp `living_total` separately from
concrete member IDs and any materialization or knowledge event. Immediate `fd_fire` only
proves ignition. When disk evidence is needed, follow [Save a checkpoint](references/live-operation.md#save-a-checkpoint); a skipped write does not justify changing the scene to force a save.
For a light-then-off test, record actual light reach and any admission at the one-minute check;
do not infer admission from the flame. After extinguishing, verify fire and residual smoke have
both ended before following the same actors.
Only for the focused closed-window smoke-onset roster probe, check the camp first, light the ordinary fire, and
inspect after one real native minute. Continue with short waits, checking smoke packet,
admission and concrete member IDs after each. Five minutes is a checkpoint, not a stop time:
keep it lit until initiation or a specific source/gate failure is established. Extinguish only
after the requested smoke observation is complete, then check residual fields. Keep each turn,
actual signal field and concrete member ID distinct; delayed materialization is a result to
measure, not assume. Full end-to-end journeys instead use meaningful interruptible intervals and
phase changes; these short probe checks are not a cadence for every suite run.

### Extinguish through Examine

For the downstairs brazier in the prepared scene, verify the player's tile is adjacent to the
actual brazier tile before opening its native Use/Examine menu with `e`. Read the menu that
appears, then select only its currently enabled letter. The native fireplace menu can offer
**Start a fire** or **Extinguish fire** according to the current `fd_fire` state and available
items; `src/iexamine.cpp::fireplace` is the source. Extinguishing needs a carried or nearby item
with bash damage. Verify `fd_fire` is absent after the selection; then follow residual smoke
and heat until the intended signal is off. An inventory search for “fire” does not open this
furniture menu. If the menu is absent, first
check adjacency, then current fire and useful items. When the current World lists
`world.examine`, `play act world.examine` opens the same native adjacent-tile chooser. Read
its returned menu and copy the current `menu.choose` target; never reuse a prior target ID.

Use the advertised `world.examine` route first. If that operation is unavailable in the bound
build, Luna may use the verified exact-PID Peekaboo `e` fallback, then select the actual native
menu entry. The R013 fallback proved extinguishing at the adjacent downstairs tile with residual
smoke/heat; it does not establish that any current CLI route is unavailable. See
[item controls](references/setup-interaction.md#gui-setup-on-this-mac) for the current host route.

## Readable play output

Ordinary `play` output should show the current game surface, relevant actors and hazards, and
the few choices available now. A menu-opening action should return that menu with its letter
choices in plain text, without a second command or raw transport JSON. Keep run IDs, hashes,
PIDs and broker internals behind diagnostics, journal or exact evidence retrieval unless one
of them is needed to resolve a current binding failure. When a worker repeats movement or opens
an unrelated menu without changing the intended state, stop and inspect the recent action
sequence and current game result before adding a macro or more output fields.

## Current reusable findings

| Finding | Scope and evidence | Use in later tests |
| --- | --- | --- |
| Debug clairvoyance has two mutations: `DEBUG_CLAIRVOYANCE_PLUS` and `DEBUG_CLAIRVOYANCE`. | `data/json/mutations/debug.json`; verified viewing setup above. | In a disposable viewing run, record both mutations, pass one real turn and verify the view. Do not credit either as a signal cause. |
| Local hostile posture reads the player's view of reserved members. Clairvoyance may change exposure and `hold_off` decisions. | `src/do_turn.cpp::live_bandit_make_gate_input` and `live_bandit_try_sight_avoid_reposition`; run `06099f7263e37a362941a1fc57bc56d550ad77539e1b8aed76d06326515a0106` is a limited viewer example. | Preserve a separate no-clairvoyance copy for concealed-player assault proof; inspect local-gate exposure logs before interpreting a visible standoff. |
| Cannibal routine members remain abstract until a valid signal admits their response. Terrain alone can create suspected leads without a scout. | `src/do_turn.cpp::live_bandit_materialize_abstract_members_for_routine`; historical R012 (raw no-fire result consumed) ran 1,441 native minutes with 12 suspected leads, no ready observer and no outing or raid. | Treat an unlit default camp with no scouts as the careful-player baseline. For post-signal routine behavior, admit a short real signal, record any knowledge it creates, extinguish the source, then follow the same NPC IDs through visits and returns. Do not call that a no-signal discovery test. |
| Debug life support and invisibility are separate traits. | `data/json/mutations/debug.json`: `DEBUG_LS` stabilizes hunger, thirst and sleepiness; `DEBUG_CLOAK` grants invisibility. | Record each trait and one real turn after activation. A cloaked travel run changes hostile sight and cannot substitute for an ordinary visibility claim. |
| A stopped wait does not establish the intended interval. | Native wait receipts and the wait method above. | Record the interruption and elapsed turns, then usually resume the remaining wait after an ordinary distraction. Stop or adapt for a consequential threat. |

## Add, prune and correct notes

Run observations, hypotheses, exact receipts and current status belong in the existing task/run
notes. There is no protocol-writing obligation after a run, successful action or discovered bug.
Current owner decisions belong in owner context/assignments, not WEC or a recipe's historical tail.

Promote a finding only when it is verified and reusable: it changes a later agent's choice of
setup, action, interpretation or recovery. Put mechanics in this manual, control syntax in the
owning control reference, and scenario-specific changes in that scenario's recipe. Replace the
obsolete instruction and remove conflicting duplicates. Keep only the condition, useful action,
expected observable result and material limitation; include a concise source/run reference where
it supports the lesson. A chronological story, full receipt/hash list or transient blocker stays
in task notes. A one-off mistake does not become a universal ban, mandatory check or approval gate.

A player proposes the concrete lesson in its normal return. The owner of the affected document
integrates it when useful, preserving other editors' custody. This is ordinary maintenance, not
a new review ceremony or prerequisite for continued play. Check changed advice against the current
controls/source and related recipes; text alone proves neither native behavior nor player adoption.
Git retains prior wording. Keep concise outcomes and reproduction steps in task context and remove
finished generated logs/saves under the existing cleanup rules; do not create protocol archives.

## Logging scope

`OPENCLAW_HARNESS_UI_TRACE` gates detailed owner trace in `src/do_turn.cpp` for harness children.
Ordinary debug-log producers can have a different scope; this does not establish that every game
diagnostic is harness-only. Use the actual run's configured traces and retain only decisive current
output. Report a demonstrated runaway producer through its existing owner rather than starting a
new logging audit or suppressing errors during a playtest.
