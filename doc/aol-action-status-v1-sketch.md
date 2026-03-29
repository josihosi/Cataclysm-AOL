# AOL Action Status / Failure Reason Sketch (v1)

This document is the implementation sketch for the first real pass of action status management in Cataclysm: Arsenic and Old Lace.
It is not the broad roadmap. That lives in [`../Plan.md`](../Plan.md).
This file answers the narrower question:

**What should the code actually look like if we want intent-driven actions to report clear status, clear failure reasons, terse player-facing refusals, and useful developer-facing debug output?**

---

## Why this exists

Right now, too many interesting NPC outcomes still collapse into one of these:
- the action silently does nothing,
- the action sort of stalls,
- the action changes course without saying why,
- or the only way to understand the result is to read code and mutter at the ceiling.

That is bad in three different ways:
- the **player** does not know why the action did not happen,
- the **developer** does not know which gate blocked it,
- and the future **automation harness** cannot assert on vague vibes.

So the first pass should create a small common action-status layer.
Not a giant universal AI theory. Just a clean contract.

---

## Scope of v1

This first pass should focus on the three action families that matter most right now:
- `look_around` pickup behavior
- `look_inventory` behavior
- `attack=<target>` behavior

These are the right first targets because they combine:
- visible gameplay importance,
- many real failure modes,
- and direct value for later smoke testing.

The status/failure-reason layer should be designed to land across the active `port/*` branches.
That does not mean every branch will accept the exact same patch with zero adjustment, because upstream divergence is real and annoying, but portability is a design goal rather than an afterthought.

---

## Core design principles

### 1. Keep the shared lifecycle small
The common state model should stay compact and readable.
It should not try to encode every edge case directly.

### 2. Put complexity into reason codes
Each action has its own ugly failure points.
That is normal.
Trying to force all of them into one giant universal enum will become silly immediately.

### 3. Separate player bark from debug detail
The player should get a short refusal or confirmation line.
The developer should get the actual cause and useful facts.
Those are related, but they are not the same output.

### 4. Prefer known blocked outcomes over generic failure
If an action was refused because there is no room, say so.
Reserve `failed` for genuinely unexpected or inconsistent outcomes.

### 5. Make the result useful for future automation
The future harness should be able to assert on:
- action kind,
- phase,
- reason code,
- and maybe a few debug facts.

That is much better than asserting on whether the screen “seemed okay”.

---

## Proposed shared model

### Shared action kind enum

```cpp
enum class llm_action_kind : int {
    none = 0,
    look_around_pickup,
    look_inventory,
    attack_target
};
```

This should stay small and focused on intent-driven actions being explicitly instrumented.
It does not need to enumerate all NPC AI behaviors in the game.

### Shared lifecycle enum

```cpp
enum class llm_action_phase : int {
    none = 0,
    requested,
    precheck,
    executing,
    waiting,
    completed,
    blocked,
    failed,
    cancelled
};
```

### Shared live status struct

```cpp
struct llm_action_status {
    llm_action_kind kind = llm_action_kind::none;
    llm_action_phase phase = llm_action_phase::none;

    std::string request_id;
    int serial = 0;

    std::string target_hint;
    std::string target_name;
    tripoint_abs_ms target_pos = tripoint_abs_ms::invalid;

    std::string reason_code;
    bool bark_sent = false;

    time_point started_turn = calendar::before_time_starts;
    time_point updated_turn = calendar::before_time_starts;

    int attempts = 0;
    std::vector<std::string> debug_facts;
};
```

### Suggested storage in `npc::llm_intent_state`

```cpp
llm_action_status active_status;
std::deque<llm_action_status> recent_statuses;
int next_action_serial = 1;
```

Recommended history retention: keep the last `8` terminal results.
That is enough for debugging and future tests without turning this into a savegame museum.

---

## Why reason codes should be strings

Use string reason codes rather than a giant shared failure enum.

Example format:
- `pickup.no_inventory_space`
- `pickup.hostile_threat_nearby`
- `inventory.item_missing`
- `attack.no_clear_shot`

That is better because it is:
- easy to extend,
- easy to log,
- easy to assert on in tests,
- easy to map to utterances,
- and does not force every action family into one giant central enum file that everyone will hate.

The common code should care about:
- action kind,
- lifecycle phase,
- terminal vs non-terminal state,
- and output emission.

The action-specific code should own the reason vocabulary.

---

## Lifecycle semantics

### `requested`
The action was accepted from parsed intent or selected as the current intent-driven action.

### `precheck`
Fast validation before pathing, spending moves, or touching deeper execution.
This is where many useful blocked states should be detected.

### `executing`
The action is actively performing the relevant step right now.
Examples:
- picking up the item,
- performing an inventory operation,
- firing or striking.

### `waiting`
The action is still alive but is currently waiting for progression.
Examples:
- walking toward target,
- aiming,
- waiting for a target to reappear briefly,
- waiting for the next turn to continue.

### `completed`
The action succeeded in the way the current step expected.

### `blocked`
A known, understandable condition prevented the action.
This is the good refusal state.
It should be the common outcome for “can’t do that now” cases.

### `failed`
The action encountered an unexpected internal or inconsistent outcome.
This should be rarer.
It is the bucket for things that probably deserve developer attention.

### `cancelled`
The action was superseded or invalidated.
Examples:
- new order arrived,
- danger forced a different priority,
- the context changed enough that continuing would be wrong.

---

## Helper API sketch

The shared helper layer should make status updates cheap and consistent.

### Begin/update/finish helpers

```cpp
void npc::begin_llm_action( llm_action_kind kind,
                            const std::string &target_hint = "",
                            const std::string &target_name = "",
                            const std::optional<tripoint_abs_ms> &target_pos = std::nullopt );

void npc::update_llm_action_phase( llm_action_phase phase,
                                   const std::string &reason_code = "",
                                   std::vector<std::string> debug_facts = {} );

void npc::finish_llm_action( llm_action_phase terminal_phase,
                             const std::string &reason_code = "",
                             std::vector<std::string> debug_facts = {} );
```

Rules:
- `finish_llm_action` should only accept terminal phases: `completed`, `blocked`, `failed`, `cancelled`.
- starting a new action should archive an old unfinished one as `cancelled` if appropriate.
- repeated updates to the same `(kind, phase, reason_code)` combination should avoid spamming the bark output.

### Output helper

```cpp
void npc::emit_llm_action_status( const llm_action_status &status ) const;
```

This should be the one place that sends action-status output to:
- structured log line(s), and
- optional debug message feed output.

That centralization matters.
Otherwise every action will log a different style of messy sentence and future debugging will be a festival of inconsistency.

---

## Output targets

### 1. Log output
Every meaningful status transition should emit a consistent line.
Not necessarily JSON, but structured enough to parse later if needed.

Example:

```text
[AOL][action_status] npc="Willy Norwood" kind="look_around_pickup" phase="blocked" reason="pickup.no_inventory_space" target="steel jerrycan" facts="volume_left=0 ml; weight_margin=120 g"
```

### 2. Debug message feed
When the relevant debug option is enabled, emit a shorter readable line into the debug feed.

Example:

```text
LLM action pickup blocked: pickup.no_inventory_space (steel jerrycan)
```

The debug feed should be concise.
The log can carry more detail.

### 3. Player-facing bark
Blocked and failed outcomes should optionally map to terse systemic bark lines.
These should be brief, repeat-safe, and not overly theatrical.

Examples:
- `pickup.no_inventory_space` -> `No room for that.`
- `pickup.hostile_threat_nearby` -> `Not while we're under threat.`
- `inventory.item_missing` -> `Don't have it.`
- `attack.no_clear_shot` -> `No clear shot.`

Use existing complaint/cooldown logic or equivalent spam control.
Do not have the NPC bark the same refusal every turn like a defective tram announcer.

---

## Initial reason-code families

## Pickup / `look_around`
Recommended first set:
- `pickup.rule_disallows`
- `pickup.zone_forbidden`
- `pickup.item_missing`
- `pickup.no_inventory_space`
- `pickup.too_heavy`
- `pickup.no_path`
- `pickup.cannot_reach`
- `pickup.hostile_threat_nearby`
- `pickup.unsafe_tile`
- `pickup.situation_changed`

Notes:
- `pickup.no_inventory_space` and `pickup.too_heavy` should stay distinct.
- `pickup.item_missing` and `pickup.situation_changed` are similar but not identical; if it was there and then conditions changed, the latter is useful for debugging.

## Inventory / `look_inventory`
Recommended first set:
- `inventory.item_missing`
- `inventory.already_equipped`
- `inventory.cannot_wield`
- `inventory.cannot_wear`
- `inventory.cannot_activate`
- `inventory.unsafe_now`
- `inventory.conflict`
- `inventory.no_suitable_item`

## Attack / `attack=<target>`
Recommended first set:
- `attack.target_missing`
- `attack.target_not_visible`
- `attack.target_invalid`
- `attack.out_of_range`
- `attack.no_clear_shot`
- `attack.friendly_fire_risk`
- `attack.no_viable_attack`
- `attack.path_blocked`
- `attack.cannot_move`
- `attack.cannot_attack`

---

## Suggested bark mapping helper

```cpp
std::string llm_action_bark_for_reason( const std::string &reason_code );
```

Initial bark map:

| Reason code | Terse bark |
| --- | --- |
| `pickup.no_inventory_space` | `No room for that.` |
| `pickup.too_heavy` | `Too heavy.` |
| `pickup.no_path` | `Can't reach it.` |
| `pickup.hostile_threat_nearby` | `Not while we're under threat.` |
| `inventory.item_missing` | `Don't have it.` |
| `inventory.cannot_wield` | `Can't wield that.` |
| `inventory.cannot_wear` | `Can't wear that.` |
| `inventory.cannot_activate` | `Can't use that.` |
| `attack.target_not_visible` | `Can't see the target.` |
| `attack.no_clear_shot` | `No clear shot.` |
| `attack.no_viable_attack` | `Can't make that attack.` |

This mapping should stay terse and systemic for now.
No need to inject personality into every refusal line yet.

---

## Likely file touch points

### `src/npc.h`
Add:
- shared enums,
- `llm_action_status` struct,
- storage in `llm_intent_state`,
- method declarations for begin/update/finish/emission helpers.

### `src/npcmove.cpp`
Add:
- helper implementations,
- pickup instrumentation,
- attack instrumentation,
- any common debug-feed emission logic.

This is probably the first and most obvious landing zone because the relevant execution flows already live here.

### `src/llm_intent.h` / `src/llm_intent.cpp`
Only put shared helper utilities here if they are genuinely cross-cutting.
Examples:
- maybe shared string formatting helpers,
- maybe log helpers,
- maybe reason-code helper functions.

Do **not** push all action-state logic into `llm_intent.*` just because the name sounds right.
If the actual execution lives in NPC movement/action code, the state transitions should stay close to that code.

---

## Instrumentation plan by action family

## Phase 1: pickup
Pickup is the best first target because it already has several clear branch points and visible failure outcomes.

### Suggested pickup flow
1. `requested`
   - begin status when a targeted pickup is selected or when the explicit intent-driven pickup operation begins.

2. `precheck`
   - validate ally rules,
   - validate no-pickup zones,
   - validate item presence,
   - validate basic carry/wear/wield possibility,
   - validate that current danger state does not obviously forbid the action.

3. `waiting`
   - NPC is moving toward the item,
   - item is known,
   - action still alive.

4. `executing`
   - item split / obtain / stash / wear / wield logic is actually being applied.

5. terminal outcome
   - `completed` on successful pickup/equip result,
   - `blocked` for known refusal conditions,
   - `failed` for weird internal mismatch or obtain failure,
   - `cancelled` if superseded.

### Pickup-specific instrumentation points in current flow
The likely first hooks are in `pick_up_item()` around:
- pickup disallowed by follower rules,
- item/zone invalidation,
- changed situation rejection,
- path failure,
- moving toward target,
- actual pickup attempt,
- zero-item result where success was expected.

---

## Phase 2: `look_inventory`
This path should report:
- whether the requested item/action was resolved,
- whether current conditions allow the inventory operation,
- and whether the operation actually changed state.

Good early blocked outcomes:
- requested item missing,
- can’t wield,
- can’t wear,
- can’t activate,
- unsafe right now.

This area will probably need a smaller resolver layer if the current logic is still spread across several item-operation branches.
That is fine. The point is not to rebuild the inventory system; the point is to expose outcomes clearly.

---

## Phase 3: `attack=<target>`
Attack instrumentation should focus on the explicit intent/override path first.

### Suggested attack flow
1. `requested`
   - a target-specific attack intent is active.

2. `precheck`
   - target exists,
   - target is visible/valid,
   - NPC is allowed to attack,
   - at least one viable attack path exists.

3. `waiting`
   - moving into range,
   - aiming,
   - briefly holding while target is reacquired.

4. `executing`
   - actual attack use/fire/strike occurs.

5. terminal outcome
   - `completed` when attack was truly spent,
   - `blocked` for clear no-shot/no-viable-attack/no-target states,
   - `failed` for inconsistent attack execution,
   - `cancelled` if target intent expires or is replaced.

### Important distinction
For ranged behavior, “I am still aiming” is **not** failure.
It is `waiting`.
Likewise, advancing toward a target is not failure.
That distinction will matter for both debugging and tests.

---

## Suggested implementation order

### Step 1
Add the shared enums, struct, storage, and helper declarations.

### Step 2
Implement central emission helpers for:
- log output,
- debug feed output,
- bark mapping.

### Step 3
Instrument pickup only.
Make sure the output feels right before spreading the pattern.

### Step 4
Add pickup bark mapping and spam control.

### Step 5
Extend the same model to `look_inventory`.

### Step 6
Extend the same model to `attack=<target>`.

### Step 7
Once the status layer feels stable, let the future harness assert on terminal outcomes.

That order matters.
If pickup instrumentation still feels clumsy, spreading it to inventory and attack will only scale the ugliness.

---

## Non-goals for this sketch

This is **not** trying to deliver:
- a universal NPC action framework,
- a full serialized action history system,
- a deep UI action inspector,
- or a full harness implementation.

It is only the sketch for the first status/result layer.

---

## Relationship to the future harness

The developer harness will become much more useful once these action outcomes exist.
Instead of asserting on fuzzy visible behavior, it can assert on things like:
- `kind=look_around_pickup`, `phase=blocked`, `reason=pickup.no_inventory_space`
- `kind=attack_target`, `phase=blocked`, `reason=attack.no_clear_shot`
- `kind=look_inventory`, `phase=completed`

That is why this status work should land before the bigger harness work.

The future harness work should still begin with the `master` branch flow.
That harness should not be assumed, by default, to be something every `port/*` branch needs immediately.
If parts of it later prove portable and worthwhile, that can be evaluated on purpose instead of by habit.

That branch policy is different from the status layer policy:
- **status/failure-reason work** should be designed for all active `port/*` branches,
- **harness work** should start on `master` and only spread when it earns the extra maintenance cost.

---

## Practical success criteria

This sketch has been implemented successfully when all of the following are true:
- pickup, inventory, and attack can report a clear lifecycle phase,
- known refusal states become explicit `blocked` results,
- player-facing refusal lines are short and understandable,
- developers can see reason codes and a few useful facts in logs/debug feed,
- and later automation can assert on structured outcomes instead of guessing from symptoms.

If that works, the project gets:
- better player feedback,
- better debugging,
- and a much better foundation for repeatable testing.
