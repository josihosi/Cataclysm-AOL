# AOL Action Status v1 Patch Plan

This document is the concrete patch plan for implementing the first pass of action status / failure-reason reporting.
It sits below the design sketch in resolution.

Related docs:
- Roadmap: [`../Plan.md`](../Plan.md)
- Design sketch: [`aol-action-status-v1-sketch.md`](aol-action-status-v1-sketch.md)

This file answers the practical question:

**Which files should be touched, in what order, and what exactly should change to land v1 without turning the codebase into stew?**

---

## Implementation goal

Land a first usable action-status layer for:
- `look_around` pickup
- `look_inventory`
- `attack=<target>`

The result should provide:
- a small shared lifecycle,
- action-specific reason codes,
- terse player-facing refusal barks,
- developer-facing log lines,
- and debug-feed visibility when the relevant debug option is enabled.

This patch plan is intentionally incremental.
The point is to land something solid and portable across the active `port/*` branches, not to perform a majestic once-and-for-all refactor while the walls quietly cave in.

---

## Current code map

Before touching anything, here is where the relevant logic lives today.

### State storage and setup
- `src/npc.h`
  - `npc::llm_intent_state`
  - `npc::llm_item_target`
  - public LLM intent methods
- `src/npc.cpp`
  - `npc::set_llm_intent_actions()`
  - `npc::clear_llm_intent_actions()`
  - `npc::set_llm_intent_move_target()`
  - `npc::set_llm_intent_item_targets()`
  - `npc::llm_intent_state_for()`

### Pickup flow
- `src/npcmove.cpp`
  - `npc::apply_llm_intent_item_targets()`
  - `npc::pick_up_item()`

### Attack flow
- `src/npcmove.cpp`
  - `npc::apply_llm_intent_target()`
  - `npc::move()`
    - especially the `attempt_llm_forced_attack` lambda
  - `npc::method_of_attack()`
  - `npc::evaluate_best_attack()`
  - `npc::execute_action()`
    - especially `case npc_do_attack`

### Inventory flow
- `src/llm_intent.cpp`
  - `apply_look_inventory_actions()`
  - `process_look_inventory_response()`
  - parser/build functions for inventory selections

### Existing LLM logging surface
- `src/llm_intent.h`
  - `llm_intent::log_event()`
- `src/llm_intent.cpp`
  - backing file append logic for `llm_intent.log`

That existing split is important.
Pickup and attack are primarily runtime NPC behavior concerns.
Inventory actions, in contrast, are currently applied directly out of the LLM response pipeline.
So the first patch series should respect that rather than pretending everything belongs in one giant file.

---

## Guiding implementation rules

### 1. Land scaffolding first, behavior second
The first patch should compile and store the new status data even if almost nothing emits it yet.
That keeps later behavior patches smaller and easier to port.

### 2. Prefer additive changes over large rewrites
Do not rewrite pickup, attack, or inventory logic from scratch.
Wrap and instrument the current flow first.

### 3. Keep terminal outcomes explicit
Whenever possible, end with one of:
- `completed`
- `blocked`
- `failed`
- `cancelled`

Do not leave intent-driven actions in a state where the only visible result is that they silently stopped being mentioned.

### 4. Use existing logging/debug surfaces before inventing new ones
For v1:
- persistent log output should reuse `llm_intent::log_event()`
- on-screen debug feed should reuse the existing `DEBUG_LLM_INTENT_UI` path unless that proves insufficient

That avoids a new option explosion for the first pass.

### 5. Make branch-porting easy
Keep the patch series small and layered.
A portable patch set matters more than one extremely elegant mega-commit that explodes on every port branch.

---

## Proposed patch series

## Patch 1: Shared scaffolding only

### Goal
Add the shared data model and helper declarations with minimal runtime behavior.
This patch should mostly be compile-safe scaffolding.

### Files
- `src/npc.h`
- `src/npc.cpp`

### Changes in `src/npc.h`
Add:
- `enum class llm_action_kind`
- `enum class llm_action_phase`
- `struct llm_action_status`
- new fields in `npc::llm_intent_state`
  - `llm_action_status active_status;`
  - `std::deque<llm_action_status> recent_statuses;`
  - `int next_action_serial = 1;`
- helper declarations, likely public or protected as appropriate:
  - `begin_llm_action(...)`
  - `update_llm_action_phase(...)`
  - `finish_llm_action(...)`
  - `clear_llm_action_status(...)`
  - maybe `has_active_llm_action_status() const`

### Changes in `src/npc.cpp`
Implement the generic state mutators:
- begin/reset/archive helpers
- history trimming
- terminal-state archiving
- safe cancellation when a new action supersedes an old one

Also update these methods to reset or preserve status correctly:
- `npc::set_llm_intent_actions()`
- `npc::clear_llm_intent_actions()`
- `npc::set_llm_intent_item_targets()`

### Specific intent of patch 1
At the end of this patch:
- the new status structures exist,
- there is a place to store current + recent action outcomes,
- clearing LLM intent also clears/cancels stale status,
- and no real behavior has been deeply altered yet.

### Checkpoint
- compile check only
- no gameplay behavior should meaningfully change yet

---

## Patch 2: Logging + debug-feed emission helpers

### Goal
Centralize action-status output before instrumenting real action paths.

### Files
- `src/npc.h`
- `src/npc.cpp`
- maybe `src/npcmove.cpp` if local helpers are cleaner there
- optionally `src/llm_intent.h` only if a tiny helper declaration becomes obviously useful

### Add helper behavior
Implement:
- `emit_llm_action_status( const llm_action_status &status )`
- `llm_action_bark_for_reason( const std::string &reason_code )`
- maybe `maybe_emit_llm_action_bark(...)`

### Logging behavior
Use `llm_intent::log_event()` to emit a structured line on phase transitions.
Format should be stable enough to grep later.

Suggested format:

```text
action_status npc="NAME" kind="look_around_pickup" phase="blocked" reason="pickup.no_inventory_space" target="steel jerrycan" facts="volume_left=0 ml; weight_margin=120 g"
```

### Debug feed behavior
If `DEBUG_LLM_INTENT_UI` is enabled, emit a short in-game message for meaningful transitions.
Keep it concise.

Suggested style:

```text
LLM pickup blocked: pickup.no_inventory_space (steel jerrycan)
```

### Bark behavior
Do **not** bark for every transition.
Only bark on terminal `blocked` / maybe selected `failed` outcomes.
Use terse systemic lines.
Respect cooldown/spam prevention.

### Checkpoint
- compile check
- ideally one synthetic call path manually exercised if possible
- still okay if no production flow uses the helpers extensively yet

---

## Patch 3: Pickup instrumentation

### Goal
Make `look_around` pickup the first fully instrumented action family.

### Files
- `src/npcmove.cpp`
- maybe `src/npc.cpp` for helper reuse if needed

### Primary functions to touch
- `npc::apply_llm_intent_item_targets()`
- `npc::pick_up_item()`
- maybe `npc::move()` at the point where targeted pickup is promoted into `npc_pickup`

### Proposed behavior changes

#### A. Start status when a concrete target is selected
When `apply_llm_intent_item_targets()` resolves a concrete item target:
- begin status with `kind = look_around_pickup`
- phase = `requested`
- populate:
  - `request_id`
  - raw `target_hint`
  - resolved `target_name`
  - `target_pos`

If no concrete item is found for a queued target, emit a terminal blocked result instead of only silently popping the queue.
Suggested reason:
- `pickup.item_missing`
  - or `pickup.no_suitable_target` if that proves clearer

#### B. Instrument precheck gates in `pick_up_item()`
At the start of `pick_up_item()` move to `precheck` and branch clearly.

Expected branch points:
- pickup disallowed by ally rule
  - `blocked: pickup.rule_disallows`
- no items remain / now in forbidden zone
  - `blocked: pickup.item_missing` or `pickup.zone_forbidden`
- situation changed since selection
  - `blocked: pickup.situation_changed`
- no path to target
  - `blocked: pickup.no_path`

Replace or wrap the existing local `log_look_around_pickup()` strings so they line up with the shared status model.

#### C. Waiting state during approach
If target is valid but still at range > 1:
- set phase to `waiting`
- include debug facts like distance and target position when cheap

This should be a genuine in-progress state, not a soft failure.

#### D. Executing state on actual pickup/equip attempt
When adjacent and actually splitting/obtaining/stashing/wearing/wielding:
- set phase to `executing`

#### E. Terminal outcomes
On success:
- `completed`

On zero-item weirdness where success was expected:
- `failed` or `blocked`, depending on cause
- likely first pass:
  - harvest path -> `completed` with a special debug fact
  - targeted pickup returning zero items unexpectedly -> `failed: pickup.obtain_failed`

#### F. Continuation handling
The current code can continue onto another target if quantity remains.
That should not erase the finished status incorrectly.

For v1:
- finish the current concrete pickup action cleanly
- then start a fresh `requested` status for the next concrete target

This is simpler and easier to read in logs than trying to model a single endless multi-target pickup super-action.

### Suggested reason codes for patch 3
Minimum useful set:
- `pickup.rule_disallows`
- `pickup.zone_forbidden`
- `pickup.item_missing`
- `pickup.situation_changed`
- `pickup.no_path`
- `pickup.no_inventory_space`
- `pickup.too_heavy`
- `pickup.obtain_failed`

Some of these may require a little extra precheck to distinguish cleanly.
That is acceptable if it stays local.

### Suggested debug facts for pickup
Only include facts that are cheap and obvious.
Examples:
- `dist=3`
- `target_pos=(x,y,z)`
- `volume_left=0 ml`
- `weight_margin=120 g`
- `wanted_quantity=2`

### Checkpoint
After patch 3, a targeted pickup should:
- visibly log lifecycle transitions,
- produce a terse refusal bark on blocked outcomes,
- and show sane debug-feed output when enabled.

This is the first real user-facing milestone.

---

## Patch 4: Inventory instrumentation

### Goal
Instrument `look_inventory` results without overcomplicating the current direct-response path.

### Important design decision
A single `look_inventory` response can request multiple operations:
- wear several items,
- wield one,
- activate another,
- drop another.

For v1, the cleanest model is:

**Treat each concrete inventory operation as its own action-status event.**

Do **not** try to force the whole batch into one single monolithic status object.
That would create awkward partial-success semantics immediately.

### Files
- `src/llm_intent.cpp`
- maybe `src/npc.h` / `src/npc.cpp` if helper signatures need one more convenience overload

### Primary functions to touch
- `apply_look_inventory_actions()`
- `process_look_inventory_response()`

### Signature changes
Change `apply_look_inventory_actions()` so it has enough context to populate status fields.
Most likely add:
- request id
- maybe source label / raw selection context if useful

Suggested signature direction:

```cpp
void apply_look_inventory_actions( npc &listener,
                                   const std::string &request_id,
                                   const std::unordered_map<std::string, item *> &inventory,
                                   const std::unordered_map<std::string, std::string> &id_to_name,
                                   const look_inventory_selection &selection );
```

### Per-operation instrumentation
For each concrete operation (`wear`, `wield`, `act`, `drop`):
1. `requested`
2. `precheck`
3. `executing`
4. terminal state

#### Example: wear
- item not found -> `blocked: inventory.item_missing`
- `can_wear()` fails -> `blocked: inventory.cannot_wear`
- `wear_item()` succeeds -> `completed`
- `wear_item()` returns no value unexpectedly -> `failed: inventory.wear_failed`

#### Example: wield
- not found -> `blocked: inventory.item_missing`
- cannot wield -> `blocked: inventory.cannot_wield`
- wield succeeds -> `completed`
- wield false -> `failed: inventory.wield_failed`

#### Example: activate
This one is messier because current code just calls `activate_item()` and logs `attempted`.
For v1, keep expectations modest.

Suggested first pass:
- not found -> `blocked: inventory.item_missing`
- if cheap precheck for activation validity exists, use it
- otherwise:
  - `executing`
  - terminal `completed` with debug fact `activation_attempted=true`

This is not perfect, but it is still more informative than the current vague “attempted”.

#### Example: drop
Similarly:
- not found -> `blocked: inventory.item_missing`
- drop attempted -> `completed` with debug fact `drop_attempted=true`
- if a reliable postcondition is easy to test, improve later

### Output style
Inventory logs should include both the item id token and resolved label when available.
That keeps logs test-friendly and human-readable.

### Checkpoint
After patch 4, `look_inventory` should no longer disappear into a pile of ad-hoc “ok / failed / attempted / not found” strings.
It should emit proper action-status records.

---

## Patch 5: Attack instrumentation

### Goal
Instrument `attack=<target>` through the current override path without rewriting the combat AI.

### Files
- `src/npcmove.cpp`
- maybe `src/npc.cpp` if one more helper is needed

### Primary functions to touch
- `npc::apply_llm_intent_target()`
- `npc::move()`
  - especially the `attempt_llm_forced_attack` lambda
- `npc::method_of_attack()`
- `npc::execute_action()`
  - especially `case npc_do_attack`

### Proposed behavior model

#### A. Begin/requested state
When an attack-target override is active and meaningful:
- begin status for `attack_target`
- set `requested`
- populate request id and target hint

This can happen either:
- when attack actions are queued in `set_llm_intent_actions()`, or
- on first use in `npc::move()` when the attack override is actually being processed

For v1, the second option is safer because it stays closer to runtime reality.

#### B. Target resolution precheck
In or near `apply_llm_intent_target()` / `attempt_llm_forced_attack`:
- no target found but grace turns remain -> `waiting`
- no target found and grace expired -> `blocked: attack.target_missing`
- target resolved and visible -> `precheck`

This matters because the current grace behavior is useful and should not be mislabeled as failure.

#### C. No viable attack path
If `method_of_attack()` returns `npc_undecided` or evaluation yields no useful attack:
- terminal `blocked: attack.no_viable_attack`

This is one of the highest-value cases to expose cleanly.

#### D. Movement / aiming as waiting
Current forced-attack flow can:
- advance toward target
- pause while trying to reacquire
- aim before firing

These should emit `waiting`, not failure.

Suggested reasons/debug facts:
- `attack.advancing`
- `attack.aiming`
- `attack.reacquire_grace`

These can be reason codes or just debug facts while phase remains `waiting`.
For v1, keep the phase stable and use debug facts unless there is a clear need for extra reason-code granularity.

#### E. Executing and completion
When `npc_do_attack` actually uses the evaluated attack:
- set `executing`
- after the use call, decide terminal outcome

For first pass, a practical definition of `completed` is:
- the attack was genuinely spent and not merely setup movement/aiming

The current code already distinguishes setup-only behavior for the attack counter.
Reuse that logic rather than inventing a different truth source.

#### F. Clear blocked outcomes worth surfacing
Priority set:
- `attack.target_missing`
- `attack.target_not_visible`
- `attack.no_clear_shot`
- `attack.friendly_fire_risk`
- `attack.no_viable_attack`
- `attack.cannot_move`
- `attack.cannot_attack`

Not all of these will be detectable from the same function.
That is fine.
Catch the easy, high-value cases first.

### Concrete hook suggestions

#### In `npc::move()` forced attack lambda
Add status transitions for:
- missing target grace -> `waiting`
- grace expired -> `blocked`
- aiming -> `waiting`
- advancing -> `waiting`

#### In `npc::method_of_attack()`
If no viable evaluated attack exists:
- emit `blocked: attack.no_viable_attack`

This may need a guard so it only emits when the LLM target override is actually active.
Do not make normal non-LLM combat spam this system accidentally.

#### In `npc::execute_action()` / `case npc_do_attack`
Add:
- `executing` before `use()`
- `completed` if the attack really spent
- `failed` if the state becomes inconsistent

### Checkpoint
After patch 5, explicit `attack=<target>` orders should produce readable lifecycle output instead of just appearing flaky when the NPC cannot line up a viable attack.

---

## Patch 6: Bark tuning and spam control

### Goal
Make blocked outputs readable to the player without turning every turn into a refusal opera.

### Files
- likely `src/npc.cpp` and/or `src/npcmove.cpp`

### Work
- route bark emission through cooldown-aware logic
- suppress repeated identical barks for the same active status
- make sure `waiting` never barks
- probably suppress routine `completed` barks for now

### Tuning rule
If in doubt:
- log it,
- debug-feed it when enabled,
- but do not bark unless the player genuinely benefits from hearing it.

---

## Patch 7: Cleanup and portability pass

### Goal
Prepare the patch series for branch propagation.

### Files
- whichever touched files remain noisy

### Work
- remove duplicated helper snippets
- normalize reason-code spelling
- make sure history trimming and cancellation behavior are consistent
- split any accidental mixed patches before porting

### Port-branch expectation
The most likely conflict hotspots across `port/*` branches will be:
- `src/npc.h`
  - `llm_intent_state` additions
- `src/npc.cpp`
  - LLM intent setup/reset methods
- `src/npcmove.cpp`
  - `pick_up_item()`
  - `apply_llm_intent_item_targets()`
  - forced-attack lambda inside `npc::move()`
- `src/llm_intent.cpp`
  - `apply_look_inventory_actions()`

That is manageable if the earlier patches stay small.

---

## Suggested commit breakdown

Recommended commit structure:

1. `npc: add llm action status scaffolding`
2. `npc: add llm action status logging helpers`
3. `npc: instrument look_around pickup status`
4. `llm_intent: instrument look_inventory status`
5. `npc: instrument attack target status`
6. `npc: tune action status barks and cooldowns`
7. `npc: clean up llm action status plumbing`

This is intentionally boring.
Boring is good.
Boring cherry-picks better.

---

## Build / verification checkpoints

## After patch 1
- compile only
- no functional expectation beyond no breakage

## After patch 3
Manual smoke check:
- ask follower to pick up a reachable item
- ask follower to pick up an unreachable or disallowed item
- verify:
  - log line exists
  - debug feed is readable when enabled
  - refusal bark is terse and not spammy

## After patch 4
Manual smoke check:
- inventory action succeeds for one item
- inventory action fails for one missing/invalid item
- verify per-operation statuses are emitted cleanly

## After patch 5
Manual smoke check:
- valid visible target attack
- invalid/missing target
- unsafe or non-viable attack
- verify waiting vs blocked is distinguished properly

## Before porting
- grep `pickup.` / `inventory.` / `attack.` reason codes for consistency
- make sure no accidental status spam occurs every turn in stable waiting loops

---

## Explicit non-goals

This patch plan is **not** trying to deliver in v1:
- a complete serialized replay of all NPC decisions
- a giant debug UI panel
- harness integration in the same patch series
- universal instrumentation for all NPC actions
- perfect activation/drop success detection for every weird inventory edge case

If those become desirable later, they should be their own passes.

---

## Recommended next action after this doc

Start with **Patch 1 + Patch 2** as a scaffold-only implementation pass.
Do not jump directly into attack instrumentation first.

Pickup should remain the first real behavior slice because it has:
- clear branch points,
- strong player value,
- and a good payoff-to-complexity ratio.

That sequence should get the project to a usable status layer without requiring heroic memory, divine revelation, or six tabs of diff archaeology.
